// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtWidgets>
#include <QDesktopServices>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <memory>

#ifndef IMAGETRANSIENT_VERSION
#define IMAGETRANSIENT_VERSION "0.2.0"
#endif

#ifndef IMAGETRANSIENT_HOMEPAGE
#define IMAGETRANSIENT_HOMEPAGE "https://github.com/yousefvand/ImageTransient"
#endif

static QString shellQuote(const QString &s)
{
    QString out = s;
    out.replace("'", "'\\''");
    return "'" + out + "'";
}

static QStringList transitionNames()
{
    return {
        "fade", "fadeblack", "fadewhite",
        "wipeleft", "wiperight", "wipeup", "wipedown",
        "slideleft", "slideright", "slideup", "slidedown",
        "circleopen", "circleclose", "radial",
        "smoothleft", "smoothright", "smoothup", "smoothdown",
        "pixelize", "dissolve", "hblur", "distance"
    };
}

static QImage fittedImage(const QImage &source, const QSize &targetSize)
{
    QImage canvas(targetSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    if (source.isNull() || targetSize.isEmpty())
        return canvas;

    const QImage scaled = source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QPoint topLeft((targetSize.width() - scaled.width()) / 2,
                         (targetSize.height() - scaled.height()) / 2);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(topLeft, scaled);
    return canvas;
}

static QPixmap blendedPreviewPixmap(const QImage &image1, const QImage &image2, const QSize &targetSize, int blendPercent)
{
    if (image1.isNull() || image2.isNull() || targetSize.isEmpty())
        return {};

    const double t = std::clamp(blendPercent, 0, 100) / 100.0;
    const QImage a = fittedImage(image1, targetSize);
    const QImage b = fittedImage(image2, targetSize);

    QImage blended(targetSize, QImage::Format_ARGB32_Premultiplied);
    blended.fill(QColor(16, 16, 16));

    QPainter painter(&blended);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(1.0 - t);
    painter.drawImage(0, 0, a);
    painter.setOpacity(t);
    painter.drawImage(0, 0, b);
    painter.end();

    return QPixmap::fromImage(blended);
}

class DropImageLabel final : public QLabel
{
public:
    explicit DropImageLabel(const QString &title, QWidget *parent = nullptr)
        : QLabel(parent), m_title(title)
    {
        setAcceptDrops(true);
        setMinimumSize(220, 130);
        setAlignment(Qt::AlignCenter);
        setFrameShape(QFrame::StyledPanel);
        setWordWrap(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setStyleSheet(QStringLiteral(
            "QLabel { border: 1px dashed palette(mid); border-radius: 6px; padding: 8px; }"));
        setPlaceholder();
    }

    void setImagePath(const QString &path)
    {
        m_path = path;
        QPixmap pix(path);
        if (pix.isNull()) {
            setText(QStringLiteral("Could not load image:\n") + path);
            setToolTip(path);
            setPixmap(QPixmap());
            return;
        }

        const QSize available(std::max(32, width() - 12), std::max(32, height() - 12));
        setPixmap(pix.scaled(available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        setToolTip(path);
    }

    QString imagePath() const { return m_path; }

    std::function<void(const QString &)> onImageDropped;

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        if (!m_path.isEmpty())
            setImagePath(m_path);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override
    {
        const auto urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            if (!url.isLocalFile())
                continue;

            const QString path = url.toLocalFile();
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (QStringList({"png", "jpg", "jpeg", "webp", "bmp", "tif", "tiff"}).contains(suffix)) {
                setImagePath(path);
                if (onImageDropped)
                    onImageDropped(path);
                event->acceptProposedAction();
                return;
            }
        }
    }

private:
    void setPlaceholder()
    {
        setText(m_title + QStringLiteral("\n\nDrop image here\nor click Browse"));
    }

    QString m_title;
    QString m_path;
};

class PreviewDialog final : public QDialog
{
public:
    PreviewDialog(const QString &image1Path, const QString &image2Path, int initialBlendPercent, QWidget *parent = nullptr)
        : QDialog(parent), m_image1(image1Path), m_image2(image2Path), m_initialBlendPercent(std::clamp(initialBlendPercent, 0, 100))
    {
        setWindowTitle(QStringLiteral("Preview transition"));
        setModal(true);
        resize(760, 520);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(8);

        m_previewLabel = new QLabel(this);
        m_previewLabel->setMinimumSize(640, 360);
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setFrameShape(QFrame::StyledPanel);
        m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        root->addWidget(m_previewLabel, 1);

        auto *captionRow = new QHBoxLayout();
        captionRow->addWidget(new QLabel(QStringLiteral("Picture 1"), this));
        captionRow->addStretch(1);
        m_percentLabel = new QLabel(QStringLiteral("0% picture 2"), this);
        m_percentLabel->setAlignment(Qt::AlignCenter);
        captionRow->addWidget(m_percentLabel);
        captionRow->addStretch(1);
        captionRow->addWidget(new QLabel(QStringLiteral("Picture 2"), this));
        root->addLayout(captionRow);

        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setRange(0, 100);
        m_slider->setValue(m_initialBlendPercent);
        m_slider->setTickPosition(QSlider::TicksBelow);
        m_slider->setTickInterval(10);
        root->addWidget(m_slider);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->addStretch(1);
        auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
        buttonRow->addWidget(closeButton);
        root->addLayout(buttonRow);

        connect(m_slider, &QSlider::valueChanged, this, [this](int value) { updatePreview(value); });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

        if (m_image1.isNull() || m_image2.isNull()) {
            m_previewLabel->setText(QStringLiteral("Could not load one or both images."));
            m_slider->setEnabled(false);
        } else {
            updatePreview(m_initialBlendPercent);
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QDialog::resizeEvent(event);
        if (m_slider && m_slider->isEnabled())
            updatePreview(m_slider->value());
    }

private:
    void updatePreview(int value)
    {
        if (!m_previewLabel || m_image1.isNull() || m_image2.isNull())
            return;

        const QSize targetSize(std::max(64, m_previewLabel->width() - 12),
                               std::max(64, m_previewLabel->height() - 12));

        m_previewLabel->setPixmap(blendedPreviewPixmap(m_image1, m_image2, targetSize, value));
        m_percentLabel->setText(QStringLiteral("%1% picture 2").arg(value));
    }

    QImage m_image1;
    QImage m_image2;
    int m_initialBlendPercent = 0;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_percentLabel = nullptr;
    QSlider *m_slider = nullptr;
};

class MainWindow final : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("Image Transient"));
        resize(840, 620);

        buildMenus();

        auto *central = new QWidget(this);
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);
        setCentralWidget(central);

        auto *headerRow = new QHBoxLayout();
        auto *title = new QLabel(QStringLiteral("Image Transient"), this);
        auto titleFont = title->font();
        titleFont.setPointSize(titleFont.pointSize() + 4);
        titleFont.setBold(true);
        title->setFont(titleFont);
        headerRow->addWidget(title);
        headerRow->addStretch(1);
        auto *aboutButton = new QPushButton(QStringLiteral("About"), this);
        auto *aboutQtButton = new QPushButton(QStringLiteral("About Qt"), this);
        headerRow->addWidget(aboutButton);
        headerRow->addWidget(aboutQtButton);
        root->addLayout(headerRow);

        auto *imageRow = new QHBoxLayout();
        imageRow->setSpacing(8);

        auto *image1Column = new QVBoxLayout();
        image1Column->setSpacing(6);
        m_image1Label = new DropImageLabel(QStringLiteral("Image 1"), this);
        auto *pick1 = new QPushButton(QStringLiteral("Browse image 1…"), this);
        pick1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        image1Column->addWidget(m_image1Label, 1);
        image1Column->addWidget(pick1);

        auto *image2Column = new QVBoxLayout();
        image2Column->setSpacing(6);
        m_image2Label = new DropImageLabel(QStringLiteral("Image 2"), this);
        auto *pick2 = new QPushButton(QStringLiteral("Browse image 2…"), this);
        pick2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        image2Column->addWidget(m_image2Label, 1);
        image2Column->addWidget(pick2);

        imageRow->addLayout(image1Column, 1);
        imageRow->addLayout(image2Column, 1);
        root->addLayout(imageRow, 1);


        auto *pickRow = new QHBoxLayout();
        m_previewButton = new QPushButton(QStringLiteral("Preview…"), this);
        m_previewButton->setEnabled(false);
        pickRow->addStretch(1);
        pickRow->addWidget(m_previewButton);
        root->addLayout(pickRow);

        auto *settingsGroup = new QGroupBox(QStringLiteral("Render settings"), this);
        auto *form = new QGridLayout(settingsGroup);
        form->setContentsMargins(10, 8, 10, 8);
        form->setHorizontalSpacing(8);
        form->setVerticalSpacing(6);

        m_transitionBox = new QComboBox(this);
        m_transitionBox->addItems(transitionNames());
        m_transitionBox->setCurrentText(QStringLiteral("fade"));

        m_hold1 = new QDoubleSpinBox(this);
        configureTimeSpin(m_hold1, 2.0);
        m_transitionDuration = new QDoubleSpinBox(this);
        configureTimeSpin(m_transitionDuration, 2.0);
        m_transitionDuration->setMinimum(0.1);
        m_hold2 = new QDoubleSpinBox(this);
        configureTimeSpin(m_hold2, 2.0);

        m_fps = new QSpinBox(this);
        m_fps->setRange(1, 120);
        m_fps->setValue(30);
        m_fps->setSuffix(QStringLiteral(" fps"));

        m_width = new QSpinBox(this);
        m_width->setRange(160, 7680);
        m_width->setValue(1920);
        m_width->setSingleStep(160);

        m_height = new QSpinBox(this);
        m_height->setRange(90, 4320);
        m_height->setValue(1080);
        m_height->setSingleStep(90);

        m_crf = new QSpinBox(this);
        m_crf->setRange(0, 51);
        m_crf->setValue(18);
        m_crf->setToolTip(QStringLiteral("Lower is higher quality. 18 is a good high-quality default."));

        m_preset = new QComboBox(this);
        m_preset->addItems({"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"});
        m_preset->setCurrentText(QStringLiteral("medium"));

        int row = 0;
        form->addWidget(new QLabel(QStringLiteral("Transition"), this), row, 0);
        form->addWidget(m_transitionBox, row, 1);
        form->addWidget(new QLabel(QStringLiteral("Hold 1"), this), row, 2);
        form->addWidget(m_hold1, row, 3);
        row++;
        form->addWidget(new QLabel(QStringLiteral("Blend"), this), row, 0);
        form->addWidget(m_transitionDuration, row, 1);
        form->addWidget(new QLabel(QStringLiteral("Hold 2"), this), row, 2);
        form->addWidget(m_hold2, row, 3);
        row++;
        form->addWidget(new QLabel(QStringLiteral("FPS"), this), row, 0);
        form->addWidget(m_fps, row, 1);
        form->addWidget(new QLabel(QStringLiteral("Size"), this), row, 2);
        auto *sizeRow = new QHBoxLayout();
        sizeRow->setContentsMargins(0, 0, 0, 0);
        sizeRow->addWidget(m_width);
        sizeRow->addWidget(new QLabel(QStringLiteral("×"), this));
        sizeRow->addWidget(m_height);
        form->addLayout(sizeRow, row, 3);
        row++;
        form->addWidget(new QLabel(QStringLiteral("CRF"), this), row, 0);
        form->addWidget(m_crf, row, 1);
        form->addWidget(new QLabel(QStringLiteral("Preset"), this), row, 2);
        form->addWidget(m_preset, row, 3);
        form->setColumnStretch(1, 1);
        form->setColumnStretch(3, 1);
        root->addWidget(settingsGroup);

        auto *outputRow = new QHBoxLayout();
        m_outputEdit = new QLineEdit(this);
        m_outputEdit->setPlaceholderText(QStringLiteral("Output .mp4 path"));
        auto *browseOutput = new QPushButton(QStringLiteral("Save as…"), this);
        outputRow->addWidget(new QLabel(QStringLiteral("Output"), this));
        outputRow->addWidget(m_outputEdit, 1);
        outputRow->addWidget(browseOutput);
        root->addLayout(outputRow);

        auto *buttonRow = new QHBoxLayout();
        m_renderButton = new QPushButton(QStringLiteral("Render MP4"), this);
        m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
        m_cancelButton->setEnabled(false);
        auto *copyCommand = new QPushButton(QStringLiteral("Copy FFmpeg command"), this);
        buttonRow->addWidget(m_renderButton);
        buttonRow->addWidget(m_cancelButton);
        buttonRow->addStretch(1);
        buttonRow->addWidget(copyCommand);
        root->addLayout(buttonRow);

        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 0);
        m_progress->setVisible(false);
        root->addWidget(m_progress);

        auto *logGroup = new QGroupBox(QStringLiteral("FFmpeg log"), this);
        logGroup->setCheckable(true);
        logGroup->setChecked(false);
        auto *logLayout = new QVBoxLayout(logGroup);
        logLayout->setContentsMargins(8, 6, 8, 8);
        m_log = new QPlainTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(5000);
        m_log->setMinimumHeight(120);
        m_log->setVisible(false);
        logLayout->addWidget(m_log);
        root->addWidget(logGroup);

        connect(pick1, &QPushButton::clicked, this, [this] { pickImage(1); });
        connect(pick2, &QPushButton::clicked, this, [this] { pickImage(2); });
        connect(m_previewButton, &QPushButton::clicked, this, [this] { showPreview(); });
        connect(browseOutput, &QPushButton::clicked, this, [this] { pickOutput(); });
        connect(m_renderButton, &QPushButton::clicked, this, [this] { render(); });
        connect(m_cancelButton, &QPushButton::clicked, this, [this] { cancelRender(); });
        connect(copyCommand, &QPushButton::clicked, this, [this] { copyCommandLine(); });
        connect(aboutButton, &QPushButton::clicked, this, [this] { showAbout(); });
        connect(aboutQtButton, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this, QStringLiteral("About Qt")); });
        connect(logGroup, &QGroupBox::toggled, m_log, &QWidget::setVisible);

        m_image1Label->onImageDropped = [this](const QString &path) {
            m_image1Path = path;
            maybeSetDefaultOutput();
            updateImageSelectionUi();
        };
        m_image2Label->onImageDropped = [this](const QString &path) {
            m_image2Path = path;
            maybeSetDefaultOutput();
            updateImageSelectionUi();
        };

        updateImageSelectionUi();
    }

private:
    static void configureTimeSpin(QDoubleSpinBox *spin, double value)
    {
        spin->setRange(0.2, 600.0);
        spin->setValue(value);
        spin->setSuffix(QStringLiteral(" s"));
        spin->setSingleStep(0.25);
        spin->setDecimals(2);
    }

    void buildMenus()
    {
        auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
        auto *aboutAction = helpMenu->addAction(QStringLiteral("About"));
        auto *aboutQtAction = helpMenu->addAction(QStringLiteral("About Qt"));
        connect(aboutAction, &QAction::triggered, this, [this] { showAbout(); });
        connect(aboutQtAction, &QAction::triggered, this, [this] { QMessageBox::aboutQt(this, QStringLiteral("About Qt")); });
    }

    void pickImage(int index)
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            index == 1 ? QStringLiteral("Choose first image") : QStringLiteral("Choose second image"),
            QDir::homePath(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff);;All files (*)"));

        if (path.isEmpty())
            return;

        if (index == 1) {
            m_image1Path = path;
            m_image1Label->setImagePath(path);
        } else {
            m_image2Path = path;
            m_image2Label->setImagePath(path);
        }
        maybeSetDefaultOutput();
        updateImageSelectionUi();
    }

    void pickOutput()
    {
        QString suggested = m_outputEdit->text().trimmed();
        if (suggested.isEmpty())
            suggested = QDir::home().filePath(QStringLiteral("image-transient.mp4"));

        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save MP4"), suggested,
            QStringLiteral("MP4 video (*.mp4);;All files (*)"));
        if (path.isEmpty())
            return;

        m_outputEdit->setText(path.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive) ? path : path + QStringLiteral(".mp4"));
    }

    void maybeSetDefaultOutput()
    {
        if (!m_outputEdit->text().trimmed().isEmpty())
            return;

        QString dir = QDir::homePath();
        if (!m_image1Path.isEmpty())
            dir = QFileInfo(m_image1Path).absolutePath();
        m_outputEdit->setText(QDir(dir).filePath(QStringLiteral("image-transient.mp4")));
    }

    bool haveBothImages() const
    {
        return !m_image1Path.isEmpty() && QFileInfo::exists(m_image1Path)
            && !m_image2Path.isEmpty() && QFileInfo::exists(m_image2Path);
    }

    void updateImageSelectionUi()
    {
        const bool ready = haveBothImages();
        if (m_previewButton)
            m_previewButton->setEnabled(ready);
    }

    bool validateImages(QString *error) const
    {
        if (m_image1Path.isEmpty() || !QFileInfo::exists(m_image1Path)) {
            *error = QStringLiteral("Choose image 1.");
            return false;
        }
        if (m_image2Path.isEmpty() || !QFileInfo::exists(m_image2Path)) {
            *error = QStringLiteral("Choose image 2.");
            return false;
        }
        return true;
    }

    bool validateRenderInputs(QString *error, bool requireFfmpeg) const
    {
        if (!validateImages(error))
            return false;
        if (m_outputEdit->text().trimmed().isEmpty()) {
            *error = QStringLiteral("Choose an output MP4 path.");
            return false;
        }
        if (requireFfmpeg && QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty()) {
            *error = QStringLiteral("FFmpeg was not found in PATH. Install it with your distribution package manager.");
            return false;
        }
        return true;
    }

    QString filterGraph() const
    {
        const int w = m_width->value();
        const int h = m_height->value();
        const int fps = m_fps->value();
        const double hold1 = m_hold1->value();
        const double hold2 = m_hold2->value();
        const double trans = m_transitionDuration->value();
        const double in1Len = hold1 + trans;
        const double in2Len = hold2 + trans;

        const QString common = QStringLiteral(
            "scale=%1:%2:force_original_aspect_ratio=decrease,"
            "pad=%1:%2:(ow-iw)/2:(oh-ih)/2,"
            "setsar=1,fps=%3,format=rgba")
            .arg(w).arg(h).arg(fps);

        return QStringLiteral(
            "[0:v]%1,trim=duration=%2,setpts=PTS-STARTPTS[v0];"
            "[1:v]%1,trim=duration=%3,setpts=PTS-STARTPTS[v1];"
            "[v0][v1]xfade=transition=%4:duration=%5:offset=%6,format=yuv420p[v]")
            .arg(common)
            .arg(in1Len, 0, 'f', 3)
            .arg(in2Len, 0, 'f', 3)
            .arg(m_transitionBox->currentText())
            .arg(trans, 0, 'f', 3)
            .arg(hold1, 0, 'f', 3);
    }

    QStringList ffmpegArgs() const
    {
        const double total = m_hold1->value() + m_transitionDuration->value() + m_hold2->value();
        return {
            "-hide_banner", "-y",
            "-loop", "1", "-i", m_image1Path,
            "-loop", "1", "-i", m_image2Path,
            "-filter_complex", filterGraph(),
            "-map", "[v]",
            "-t", QString::number(total, 'f', 3),
            "-an",
            "-c:v", "libx264",
            "-preset", m_preset->currentText(),
            "-crf", QString::number(m_crf->value()),
            "-movflags", "+faststart",
            m_outputEdit->text().trimmed()
        };
    }

    QString commandLine() const
    {
        QStringList quoted;
        quoted << QStringLiteral("ffmpeg");
        for (const QString &arg : ffmpegArgs())
            quoted << shellQuote(arg);
        return quoted.join(QLatin1Char(' '));
    }

    void showPreview()
    {
        QString error;
        if (!validateImages(&error)) {
            QMessageBox::warning(this, QStringLiteral("Cannot preview"), error);
            return;
        }

        PreviewDialog dialog(m_image1Path, m_image2Path, 0, this);
        dialog.exec();
    }

    void render()
    {
        QString error;
        if (!validateRenderInputs(&error, true)) {
            QMessageBox::warning(this, QStringLiteral("Cannot render"), error);
            return;
        }

        const QString out = m_outputEdit->text().trimmed();
        QDir().mkpath(QFileInfo(out).absolutePath());

        m_log->appendPlainText(QStringLiteral("Running:\n") + commandLine() + QStringLiteral("\n"));
        setRenderingUi(true);

        m_process = std::make_unique<QProcess>();
        m_process->setProgram(QStringLiteral("ffmpeg"));
        m_process->setArguments(ffmpegArgs());
        m_process->setProcessChannelMode(QProcess::MergedChannels);

        connect(m_process.get(), &QProcess::readyReadStandardOutput, this, [this] {
            if (m_process)
                m_log->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
        });

        connect(m_process.get(), &QProcess::finished, this, [this, out](int exitCode, QProcess::ExitStatus status) {
            setRenderingUi(false);
            if (status == QProcess::NormalExit && exitCode == 0) {
                m_log->appendPlainText(QStringLiteral("\nDone: ") + out);
                QMessageBox::information(this, QStringLiteral("Render complete"), QStringLiteral("Video created:\n") + out);
            } else {
                m_log->appendPlainText(QStringLiteral("\nFFmpeg failed. Exit code: ") + QString::number(exitCode));
                QMessageBox::critical(this, QStringLiteral("Render failed"), QStringLiteral("FFmpeg failed. See the log for details."));
            }
            m_process.reset();
        });

        connect(m_process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            setRenderingUi(false);
            m_log->appendPlainText(QStringLiteral("\nProcess error: ") + QString::number(static_cast<int>(e)));
        });

        m_process->start();
    }

    void cancelRender()
    {
        if (!m_process)
            return;

        m_log->appendPlainText(QStringLiteral("\nCancelling…"));
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }

    void copyCommandLine()
    {
        QString error;
        if (!validateRenderInputs(&error, false)) {
            QMessageBox::warning(this, QStringLiteral("Cannot copy command"), error);
            return;
        }

        QApplication::clipboard()->setText(commandLine());
        m_log->appendPlainText(QStringLiteral("Copied FFmpeg command to clipboard.\n"));
    }

    void showAbout()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("About Image Transient"));
        dialog.setModal(true);
        dialog.setMinimumWidth(420);

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(10);

        auto *name = new QLabel(QStringLiteral("<h2>Image Transient</h2>"), &dialog);
        name->setTextFormat(Qt::RichText);
        layout->addWidget(name);

        layout->addWidget(new QLabel(QStringLiteral("Version %1").arg(QString::fromUtf8(IMAGETRANSIENT_VERSION)), &dialog));

        auto *link = new QLabel(QStringLiteral("<a href=\"") + QString::fromUtf8(IMAGETRANSIENT_HOMEPAGE) + QStringLiteral("\">") + QString::fromUtf8(IMAGETRANSIENT_HOMEPAGE) + QStringLiteral("</a>"), &dialog);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(true);
        layout->addWidget(link);

        auto *license = new QLabel(QStringLiteral("License: GPL-3.0-or-later"), &dialog);
        layout->addWidget(license);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        layout->addWidget(buttons);

        dialog.exec();
    }

    void setRenderingUi(bool rendering)
    {
        m_renderButton->setEnabled(!rendering);
        m_cancelButton->setEnabled(rendering);
        m_progress->setVisible(rendering);
    }

    QString m_image1Path;
    QString m_image2Path;

    DropImageLabel *m_image1Label = nullptr;
    DropImageLabel *m_image2Label = nullptr;
    QPushButton *m_previewButton = nullptr;
    QComboBox *m_transitionBox = nullptr;
    QDoubleSpinBox *m_hold1 = nullptr;
    QDoubleSpinBox *m_transitionDuration = nullptr;
    QDoubleSpinBox *m_hold2 = nullptr;
    QSpinBox *m_fps = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QSpinBox *m_crf = nullptr;
    QComboBox *m_preset = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QPlainTextEdit *m_log = nullptr;
    std::unique_ptr<QProcess> m_process;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Image Transient"));
    QApplication::setApplicationDisplayName(QStringLiteral("Image Transient"));
    QApplication::setApplicationVersion(QString::fromUtf8(IMAGETRANSIENT_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Remisa"));

    MainWindow window;
    window.show();
    return app.exec();
}
