#include "DemoRecorder.h"

#include "third_party/h264_config.h"
#include "third_party/minih264e.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <QWaitCondition>
#include <QWidget>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int align16(int v)
{
    return (v + 15) & ~15;
}

constexpr int kRecordFps = 60;
constexpr int kMaxQueuedFrames = 180;

void appendU16(QByteArray &b, quint16 v)
{
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}

void appendU32(QByteArray &b, quint32 v)
{
    b.append(char((v >> 24) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}

void appendFourcc(QByteArray &b, const char *cc)
{
    b.append(cc, 4);
}

QByteArray makeBox(const char *type, const QByteArray &payload)
{
    QByteArray box;
    appendU32(box, quint32(8 + payload.size()));
    appendFourcc(box, type);
    box.append(payload);
    return box;
}

QByteArray makeFullBox(const char *type, quint8 version, quint32 flags, const QByteArray &payload)
{
    QByteArray body;
    body.append(char(version));
    body.append(char((flags >> 16) & 0xFF));
    body.append(char((flags >> 8) & 0xFF));
    body.append(char(flags & 0xFF));
    body.append(payload);
    return makeBox(type, body);
}

class SimpleMp4 {
public:
    explicit SimpleMp4(int width, int height, int fps)
        : m_width(width)
        , m_height(height)
        , m_fps(fps > 0 ? fps : 30)
    {
    }

    void addAnnexB(const unsigned char *data, int size)
    {
        QByteArray sample;
        bool key = false;
        int i = 0;
        while (i + 3 < size) {
            int sc = 0;
            if (i + 3 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
                sc = 3;
            else if (i + 4 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
                sc = 4;
            else {
                ++i;
                continue;
            }
            const int start = i + sc;
            int next = start;
            while (next + 3 < size) {
                if (data[next] == 0 && data[next + 1] == 0 && data[next + 2] == 1)
                    break;
                if (next + 4 < size && data[next] == 0 && data[next + 1] == 0
                    && data[next + 2] == 0 && data[next + 3] == 1)
                    break;
                ++next;
            }
            if (next + 3 >= size)
                next = size;
            const int naluSize = next - start;
            if (naluSize > 0)
                consumeNal(data + start, naluSize, sample, key);
            i = next;
        }
        if (sample.isEmpty())
            return;
        m_samples.append(sample);
        m_keys.append(key);
    }

    bool writeFile(FILE *file) const
    {
        if (m_samples.isEmpty() || m_sps.isEmpty() || m_pps.isEmpty())
            return false;

        QByteArray ftypPayload;
        appendFourcc(ftypPayload, "isom");
        appendU32(ftypPayload, 0x00000200);
        appendFourcc(ftypPayload, "isom");
        appendFourcc(ftypPayload, "iso2");
        appendFourcc(ftypPayload, "avc1");
        appendFourcc(ftypPayload, "mp41");
        const QByteArray ftyp = makeBox("ftyp", ftypPayload);

        QByteArray mdatPayload;
        for (const QByteArray &s : m_samples)
            mdatPayload.append(s);
        const QByteArray mdat = makeBox("mdat", mdatPayload);
        QByteArray moov = buildMoov(0);
        const quint32 mdatDataOffset = quint32(ftyp.size() + moov.size() + 8);
        moov = buildMoov(mdatDataOffset);

        if (fwrite(ftyp.constData(), 1, size_t(ftyp.size()), file) != size_t(ftyp.size()))
            return false;
        if (fwrite(moov.constData(), 1, size_t(moov.size()), file) != size_t(moov.size()))
            return false;
        if (fwrite(mdat.constData(), 1, size_t(mdat.size()), file) != size_t(mdat.size()))
            return false;
        return true;
    }

private:
    void consumeNal(const unsigned char *nal, int naluSize, QByteArray &sample, bool &key)
    {
        const int type = nal[0] & 0x1F;
        if (type == 7) {
            m_sps = QByteArray(reinterpret_cast<const char *>(nal), naluSize);
            return;
        }
        if (type == 8) {
            m_pps = QByteArray(reinterpret_cast<const char *>(nal), naluSize);
            return;
        }
        if (type == 5)
            key = true;
        if (type == 9)
            return;
        appendU32(sample, quint32(naluSize));
        sample.append(reinterpret_cast<const char *>(nal), naluSize);
    }

    QByteArray avcC() const
    {
        QByteArray b;
        b.append(char(1));
        b.append(m_sps.size() > 3 ? m_sps[1] : char(0x42));
        b.append(m_sps.size() > 3 ? m_sps[2] : char(0));
        b.append(m_sps.size() > 3 ? m_sps[3] : char(0x1E));
        b.append(char(0xFF));
        b.append(char(0xE1));
        appendU16(b, quint16(m_sps.size()));
        b.append(m_sps);
        b.append(char(1));
        appendU16(b, quint16(m_pps.size()));
        b.append(m_pps);
        return makeBox("avcC", b);
    }

    QByteArray buildMoov(quint32 mdatPayloadOffset) const
    {
        const int n = m_samples.size();
        const quint32 duration = quint32(n);
        const quint32 timescale = quint32(m_fps);

        QByteArray mvhd;
        appendU32(mvhd, 0);
        appendU32(mvhd, 0);
        appendU32(mvhd, timescale);
        appendU32(mvhd, duration);
        appendU32(mvhd, 0x00010000);
        appendU16(mvhd, 0x0100);
        appendU16(mvhd, 0);
        appendU32(mvhd, 0);
        appendU32(mvhd, 0);
        const quint32 matrix[] = {
            0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000
        };
        for (quint32 m : matrix)
            appendU32(mvhd, m);
        for (int i = 0; i < 6; ++i)
            appendU32(mvhd, 0);
        appendU32(mvhd, 2);

        QByteArray tkhd;
        appendU32(tkhd, 0);
        appendU32(tkhd, 0);
        appendU32(tkhd, 1);
        appendU32(tkhd, 0);
        appendU32(tkhd, duration);
        appendU32(tkhd, 0);
        appendU32(tkhd, 0);
        appendU16(tkhd, 0);
        appendU16(tkhd, 0);
        appendU16(tkhd, 0);
        appendU16(tkhd, 0);
        for (quint32 m : matrix)
            appendU32(tkhd, m);
        appendU32(tkhd, quint32(m_width) << 16);
        appendU32(tkhd, quint32(m_height) << 16);

        QByteArray mdhd;
        appendU32(mdhd, 0);
        appendU32(mdhd, 0);
        appendU32(mdhd, timescale);
        appendU32(mdhd, duration);
        appendU16(mdhd, 0x55C4);
        appendU16(mdhd, 0);

        QByteArray hdlr;
        appendU32(hdlr, 0);
        appendFourcc(hdlr, "vide");
        appendU32(hdlr, 0);
        appendU32(hdlr, 0);
        appendU32(hdlr, 0);
        hdlr.append("VideoHandler", 13);

        QByteArray vmhd;
        appendU16(vmhd, 0);
        appendU16(vmhd, 0);
        appendU16(vmhd, 0);
        appendU16(vmhd, 0);

        QByteArray dref = makeFullBox("url ", 0, 1, QByteArray());
        QByteArray drefBody;
        appendU32(drefBody, 1);
        drefBody.append(dref);

        QByteArray avc1;
        avc1.append(6, char(0));
        appendU16(avc1, 1);
        appendU16(avc1, 0);
        appendU16(avc1, 0);
        appendU32(avc1, 0);
        appendU32(avc1, 0);
        appendU32(avc1, 0);
        appendU16(avc1, quint16(m_width));
        appendU16(avc1, quint16(m_height));
        appendU32(avc1, 0x00480000);
        appendU32(avc1, 0x00480000);
        appendU32(avc1, 0);
        appendU16(avc1, 1);
        avc1.append(32, char(0));
        appendU16(avc1, 0x0018);
        appendU16(avc1, 0xFFFF);
        avc1.append(avcC());

        QByteArray stsdBody;
        appendU32(stsdBody, 1);
        stsdBody.append(makeBox("avc1", avc1));

        QByteArray sttsBody;
        appendU32(sttsBody, 1);
        appendU32(sttsBody, quint32(n));
        appendU32(sttsBody, 1);

        QByteArray stscBody;
        appendU32(stscBody, 1);
        appendU32(stscBody, 1);
        appendU32(stscBody, quint32(n));
        appendU32(stscBody, 1);

        QByteArray stszBody;
        appendU32(stszBody, 0);
        appendU32(stszBody, quint32(n));
        for (const QByteArray &s : m_samples)
            appendU32(stszBody, quint32(s.size()));

        QByteArray stcoBody;
        appendU32(stcoBody, 1);
        appendU32(stcoBody, mdatPayloadOffset);

        QByteArray stssBody;
        QVector<int> keys;
        for (int i = 0; i < m_keys.size(); ++i) {
            if (m_keys.at(i))
                keys.append(i + 1);
        }
        if (keys.isEmpty())
            keys.append(1);
        appendU32(stssBody, quint32(keys.size()));
        for (int idx : keys)
            appendU32(stssBody, quint32(idx));

        const QByteArray stbl = makeBox("stbl",
            makeFullBox("stsd", 0, 0, stsdBody)
            + makeFullBox("stts", 0, 0, sttsBody)
            + makeFullBox("stsc", 0, 0, stscBody)
            + makeFullBox("stsz", 0, 0, stszBody)
            + makeFullBox("stco", 0, 0, stcoBody)
            + makeFullBox("stss", 0, 0, stssBody));

        const QByteArray minf = makeBox("minf",
            makeFullBox("vmhd", 0, 1, vmhd)
            + makeBox("dinf", makeFullBox("dref", 0, 0, drefBody))
            + stbl);

        const QByteArray mdia = makeBox("mdia",
            makeFullBox("mdhd", 0, 0, mdhd)
            + makeFullBox("hdlr", 0, 0, hdlr)
            + minf);

        const QByteArray trak = makeBox("trak",
            makeFullBox("tkhd", 0, 3, tkhd)
            + mdia);

        return makeBox("moov", makeFullBox("mvhd", 0, 0, mvhd) + trak);
    }

    int m_width;
    int m_height;
    int m_fps;
    QByteArray m_sps;
    QByteArray m_pps;
    QVector<QByteArray> m_samples;
    QVector<bool> m_keys;
};

unsigned char clampByte(int v)
{
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return static_cast<unsigned char>(v);
}

void rgb32ToYuv420(const QImage &img, unsigned char *y, unsigned char *u, unsigned char *v,
                   int encW, int encH)
{
    const int srcW = img.width();
    const int srcH = img.height();
    const int bpl = img.bytesPerLine();
    const uchar *bits = img.constBits();

    const int copyH = qMin(srcH, encH);
    const int copyW = qMin(srcW, encW);
    if (copyW <= 0 || copyH <= 0) {
        memset(y, 16, encW * encH);
        memset(u, 128, (encW / 2) * (encH / 2));
        memset(v, 128, (encW / 2) * (encH / 2));
        return;
    }

    for (int row = 0; row < copyH; ++row) {
        const uchar *src = bits + row * bpl;
        unsigned char *yd = y + row * encW;
        for (int col = 0; col < copyW; ++col) {
            const int b = src[col * 4 + 0];
            const int g = src[col * 4 + 1];
            const int r = src[col * 4 + 2];
            yd[col] = clampByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
        const unsigned char edge = yd[copyW - 1];
        for (int col = copyW; col < encW; ++col)
            yd[col] = edge;
    }
    for (int row = copyH; row < encH; ++row)
        memcpy(y + row * encW, y + (copyH - 1) * encW, size_t(encW));

    const int copyChromaH = (copyH + 1) / 2;
    const int copyChromaW = (copyW + 1) / 2;
    const int encChromaW = encW / 2;
    const int encChromaH = encH / 2;

    for (int row = 0; row < copyH; row += 2) {
        const uchar *src0 = bits + row * bpl;
        const uchar *src1 = bits + qMin(row + 1, copyH - 1) * bpl;
        unsigned char *ud = u + (row / 2) * encChromaW;
        unsigned char *vd = v + (row / 2) * encChromaW;
        for (int col = 0; col < copyW; col += 2) {
            int b = 0, g = 0, r = 0;
            for (int dy = 0; dy < 2; ++dy) {
                const uchar *s = (dy == 0) ? src0 : src1;
                for (int dx = 0; dx < 2; ++dx) {
                    const int x = qMin(col + dx, copyW - 1) * 4;
                    b += s[x + 0];
                    g += s[x + 1];
                    r += s[x + 2];
                }
            }
            b >>= 2;
            g >>= 2;
            r >>= 2;
            ud[col / 2] = clampByte(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            vd[col / 2] = clampByte(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        }
        const unsigned char uEdge = ud[copyChromaW - 1];
        const unsigned char vEdge = vd[copyChromaW - 1];
        for (int col = copyChromaW; col < encChromaW; ++col) {
            ud[col] = uEdge;
            vd[col] = vEdge;
        }
    }
    if (copyChromaH > 0) {
        for (int row = copyChromaH; row < encChromaH; ++row) {
            memcpy(u + row * encChromaW, u + (copyChromaH - 1) * encChromaW, size_t(encChromaW));
            memcpy(v + row * encChromaW, v + (copyChromaH - 1) * encChromaW, size_t(encChromaW));
        }
    }
}

} // namespace

class DemoEncodeThread : public QThread {
public:
    explicit DemoEncodeThread(const QString &path, int width, int height)
        : m_path(path)
        , m_srcW(width & ~1)
        , m_srcH(height & ~1)
        , m_encW(align16(m_srcW))
        , m_encH(align16(m_srcH))
    {
    }

    void push(const QImage &img)
    {
        QMutexLocker lock(&m_mutex);
        if (m_queue.size() > kMaxQueuedFrames)
            return;
        m_queue.enqueue(img);
        m_cond.wakeOne();
    }

    void requestStop()
    {
        QMutexLocker lock(&m_mutex);
        m_stop = true;
        m_cond.wakeOne();
    }

protected:
    void run() override
    {
#ifdef _WIN32
        FILE *file = _wfopen(reinterpret_cast<const wchar_t *>(m_path.utf16()), L"wb");
#else
        FILE *file = fopen(m_path.toLocal8Bit().constData(), "wb");
#endif
        if (!file)
            return;

        H264E_create_param_t create;
        memset(&create, 0, sizeof(create));
        create.width = m_srcW;
        create.height = m_srcH;
        create.gop = kRecordFps;
        create.const_input_flag = 1;
        create.fine_rate_control_flag = 0;
        create.vbv_size_bytes = 16 * 1000 * 1000;
        create.vbv_overflow_empty_frame_flag = 0;
        create.vbv_underflow_stuffing_flag = 0;
        create.temporal_denoise_flag = 0;

        int persistSize = 0;
        int scratchSize = 0;
        if (H264E_sizeof(&create, &persistSize, &scratchSize) != H264E_STATUS_SUCCESS) {
            fclose(file);
            return;
        }

        H264E_persist_t *enc = static_cast<H264E_persist_t *>(malloc(persistSize));
        H264E_scratch_t *scratch = static_cast<H264E_scratch_t *>(malloc(scratchSize));
        if (!enc || !scratch || H264E_init(enc, &create) != H264E_STATUS_SUCCESS) {
            free(enc);
            free(scratch);
            fclose(file);
            return;
        }

        const int ySize = m_encW * m_encH;
        const int uvSize = (m_encW / 2) * (m_encH / 2);
        unsigned char *yuv = static_cast<unsigned char *>(malloc(ySize + uvSize * 2));
        if (!yuv) {
            free(enc);
            free(scratch);
            fclose(file);
            return;
        }
        unsigned char *yPlane = yuv;
        unsigned char *uPlane = yuv + ySize;
        unsigned char *vPlane = uPlane + uvSize;

        SimpleMp4 mp4(m_srcW, m_srcH, kRecordFps);
        bool first = true;

        while (true) {
            QImage img;
            {
                QMutexLocker lock(&m_mutex);
                while (m_queue.isEmpty() && !m_stop)
                    m_cond.wait(&m_mutex);
                if (m_queue.isEmpty() && m_stop)
                    break;
                img = m_queue.dequeue();
            }

            if (img.format() != QImage::Format_RGB32)
                img = img.convertToFormat(QImage::Format_RGB32);
            rgb32ToYuv420(img, yPlane, uPlane, vPlane, m_encW, m_encH);

            H264E_io_yuv_t frame;
            memset(&frame, 0, sizeof(frame));
            frame.yuv[0] = yPlane;
            frame.yuv[1] = uPlane;
            frame.yuv[2] = vPlane;
            frame.stride[0] = m_encW;
            frame.stride[1] = m_encW / 2;
            frame.stride[2] = m_encW / 2;

            H264E_run_param_t run;
            memset(&run, 0, sizeof(run));
            run.encode_speed = 1;
            run.frame_type = first ? H264E_FRAME_TYPE_KEY : H264E_FRAME_TYPE_DEFAULT;
            run.desired_frame_bytes = 80000;
            run.qp_min = 12;
            run.qp_max = 24;

            unsigned char *coded = nullptr;
            int codedSize = 0;
            if (H264E_encode(enc, scratch, &run, &frame, &coded, &codedSize) != H264E_STATUS_SUCCESS)
                continue;
            if (coded && codedSize > 0)
                mp4.addAnnexB(coded, codedSize);
            first = false;
        }

        mp4.writeFile(file);
        free(yuv);
        free(enc);
        free(scratch);
        fclose(file);
    }

private:
    QString m_path;
    int m_srcW;
    int m_srcH;
    int m_encW;
    int m_encH;
    QMutex m_mutex;
    QWaitCondition m_cond;
    QQueue<QImage> m_queue;
    bool m_stop { false };
};

DemoRecorder::DemoRecorder(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(8);
    connect(m_timer, &QTimer::timeout, this, &DemoRecorder::onTick);
}

DemoRecorder::~DemoRecorder()
{
    stop();
}

bool DemoRecorder::start(QWidget *window, const QString &filePrefix)
{
    if (m_running || !window)
        return false;
    m_window = window;
    const QSize sz = window->size();
    if (sz.width() < 16 || sz.height() < 16)
        return false;

    const QString dir = QCoreApplication::applicationDirPath();
    const QString prefix = filePrefix.isEmpty() ? QStringLiteral("Demo") : filePrefix;
    m_outputPath = QDir(dir).filePath(
        QStringLiteral("%1_%2.mp4")
            .arg(prefix, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));

    m_thread = new DemoEncodeThread(m_outputPath, sz.width(), sz.height());
    m_thread->start();
    m_frameIndex = 0;
    m_running = true;
    m_clock.start();
    m_timer->start();
    onTick();
    return true;
}

void DemoRecorder::stop()
{
    if (!m_running)
        return;
    m_timer->stop();
    m_running = false;
    if (m_thread) {
        m_thread->requestStop();
        m_thread->wait(60000);
        delete m_thread;
        m_thread = nullptr;
    }
    m_window = nullptr;
}

void DemoRecorder::onTick()
{
    if (!m_running || !m_window || !m_thread)
        return;

    const qint64 now = m_clock.elapsed();
    const qint64 due = m_frameIndex * 1000 / kRecordFps;
    if (now < due)
        return;

    QImage img = m_window->grab().toImage();
    if (img.isNull())
        return;
    if (img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(QImage::Format_RGB32);

    while (m_frameIndex * 1000 / kRecordFps <= now) {
        m_thread->push(img);
        m_frameIndex++;
        if (m_frameIndex > kRecordFps * 180)
            break;
    }
}
