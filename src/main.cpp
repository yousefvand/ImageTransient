#include <QtWidgets>
#include <QProcess>
#include <QStandardPaths>
#include <cmath>
#include <memory>

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

class DropImageLabel final : public QLabel
{
public:
    explicit DropImageLabel(const QString &title, QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAcceptDrops(true);
        setMinimumSize(260, 180);
        setAlignment(Qt::AlignCenter);
        setFrameShape(QFrame::StyledPanel);
        setText(title + "\n\nDrop image here\nor click Browse");
        setWordWrap(true);
        setStyleSheet("QLabel { border: 1px dashed palette(mid); border-radius: 8px; padding: 12px; }");
    }

    void setImagePath(const QString &path)
    {
        m_path = path;
        QPixmap pix(path);
        if (pix.isNull()) {
            setText("Could not load image:\n" + path);
            setToolTip(path);
            return;
        }
        setPixmap(pix.scaled(size() - QSize(16, 16), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        setToolTip(path);
    }

    QString imagePath() const { return m_path; }

    std::function<void(const QString&)> onImageDropped;

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
    QString m_path;
};

class MainWindow final : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle("Image Transient");
        resize(980, 720);

        auto *central = new QWidget(this);
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(16, 16, 16, 16);
        root->setSpacing(12);
        setCentralWidget(central);

        auto *title = new QLabel("Image Transient", this);
        auto titleFont = title->font();
        titleFont.setPointSize(titleFont.pointSize() + 8);
        titleFont.setBold(true);
        title->setFont(titleFont);

        auto *subtitle = new QLabel("Create a short MP4 transition video from two still images. Works on KDE Wayland because rendering is handled by FFmpeg, not screen capture.", this);
        subtitle->setWordWrap(true);

        root->addWidget(title);
        root->addWidget(subtitle);

        auto *imageRow = new QHBoxLayout();
        m_image1Label = new DropImageLabel("Image 1", this);
        m_image2Label = new DropImageLabel("Image 2", this);
        imageRow->addWidget(m_image1Label, 1);
        imageRow->addWidget(m_image2Label, 1);
        root->addLayout(imageRow, 2);

        auto *pickRow = new QHBoxLayout();
        auto *pick1 = new QPushButton("Browse image 1…", this);
        auto *pick2 = new QPushButton("Browse image 2…", this);
        pickRow->addWidget(pick1);
        pickRow->addWidget(pick2);
        root->addLayout(pickRow);

        auto *settingsGroup = new QGroupBox("Render settings", this);
        auto *form = new QGridLayout(settingsGroup);

        m_transitionBox = new QComboBox(this);
        m_transitionBox->addItems(transitionNames());
        m_transitionBox->setCurrentText("fade");

        m_hold1 = new QDoubleSpinBox(this);
        m_hold1->setRange(0.2, 600.0);
        m_hold1->setValue(2.0);
        m_hold1->setSuffix(" s");
        m_hold1->setSingleStep(0.25);

        m_transitionDuration = new QDoubleSpinBox(this);
        m_transitionDuration->setRange(0.1, 120.0);
        m_transitionDuration->setValue(2.0);
        m_transitionDuration->setSuffix(" s");
        m_transitionDuration->setSingleStep(0.25);

        m_hold2 = new QDoubleSpinBox(this);
        m_hold2->setRange(0.2, 600.0);
        m_hold2->setValue(2.0);
        m_hold2->setSuffix(" s");
        m_hold2->setSingleStep(0.25);

        m_fps = new QSpinBox(this);
        m_fps->setRange(1, 120);
        m_fps->setValue(30);
        m_fps->setSuffix(" fps");

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
        m_crf->setToolTip("Lower is higher quality. 18 is visually high quality; 23 is default-ish; 0 is lossless-ish but huge.");

        m_preset = new QComboBox(this);
        m_preset->addItems({"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"});
        m_preset->setCurrentText("medium");

        int row = 0;
        form->addWidget(new QLabel("Transition"), row, 0);
        form->addWidget(m_transitionBox, row, 1);
        form->addWidget(new QLabel("First image hold"), row, 2);
        form->addWidget(m_hold1, row, 3);
        row++;

        form->addWidget(new QLabel("Transition duration"), row, 0);
        form->addWidget(m_transitionDuration, row, 1);
        form->addWidget(new QLabel("Second image hold"), row, 2);
        form->addWidget(m_hold2, row, 3);
        row++;

        form->addWidget(new QLabel("FPS"), row, 0);
        form->addWidget(m_fps, row, 1);
        form->addWidget(new QLabel("Output size"), row, 2);
        auto *sizeRow = new QHBoxLayout();
        sizeRow->addWidget(m_width);
        sizeRow->addWidget(new QLabel("×", this));
        sizeRow->addWidget(m_height);
        form->addLayout(sizeRow, row, 3);
        row++;

        form->addWidget(new QLabel("H.264 quality CRF"), row, 0);
        form->addWidget(m_crf, row, 1);
        form->addWidget(new QLabel("Encoder preset"), row, 2);
        form->addWidget(m_preset, row, 3);
        row++;

        form->setColumnStretch(1, 1);
        form->setColumnStretch(3, 1);
        root->addWidget(settingsGroup);

        auto *outputRow = new QHBoxLayout();
        m_outputEdit = new QLineEdit(this);
        m_outputEdit->setPlaceholderText("Output .mp4 path");
        auto *browseOutput = new QPushButton("Save as…", this);
        outputRow->addWidget(m_outputEdit, 1);
        outputRow->addWidget(browseOutput);
        root->addLayout(outputRow);

        auto *buttonRow = new QHBoxLayout();
        m_renderButton = new QPushButton("Render MP4", this);
        m_cancelButton = new QPushButton("Cancel", this);
        m_cancelButton->setEnabled(false);
        auto *copyCommand = new QPushButton("Copy FFmpeg command", this);
        buttonRow->addWidget(m_renderButton);
        buttonRow->addWidget(m_cancelButton);
        buttonRow->addStretch(1);
        buttonRow->addWidget(copyCommand);
        root->addLayout(buttonRow);

        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 0);
        m_progress->setVisible(false);
        root->addWidget(m_progress);

        m_log = new QPlainTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(5000);
        root->addWidget(m_log, 1);

        connect(pick1, &QPushButton::clicked, this, [this] { pickImage(1); });
        connect(pick2, &QPushButton::clicked, this, [this] { pickImage(2); });
        connect(browseOutput, &QPushButton::clicked, this, [this] { pickOutput(); });
        connect(m_renderButton, &QPushButton::clicked, this, [this] { render(); });
        connect(m_cancelButton, &QPushButton::clicked, this, [this] { cancelRender(); });
        connect(copyCommand, &QPushButton::clicked, this, [this] { copyCommandLine(); });

        m_image1Label->onImageDropped = [this](const QString &path) { m_image1Path = path; maybeSetDefaultOutput(); };
        m_image2Label->onImageDropped = [this](const QString &path) { m_image2Path = path; maybeSetDefaultOutput(); };
    }

private:
    void pickImage(int index)
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            index == 1 ? "Choose first image" : "Choose second image",
            QDir::homePath(),
            "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff);;All files (*)"
        );
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
    }

    void pickOutput()
    {
        QString suggested = m_outputEdit->text().trimmed();
        if (suggested.isEmpty())
            suggested = QDir::home().filePath("image-transient.mp4");
        const QString path = QFileDialog::getSaveFileName(this, "Save MP4", suggested, "MP4 video (*.mp4);;All files (*)");
        if (path.isEmpty())
            return;
        m_outputEdit->setText(path.endsWith(".mp4", Qt::CaseInsensitive) ? path : path + ".mp4");
    }

    void maybeSetDefaultOutput()
    {
        if (!m_outputEdit->text().trimmed().isEmpty())
            return;
        QString dir = QDir::homePath();
        if (!m_image1Path.isEmpty())
            dir = QFileInfo(m_image1Path).absolutePath();
        m_outputEdit->setText(QDir(dir).filePath("image-transient.mp4"));
    }

    bool validateInputs(QString *error) const
    {
        if (m_image1Path.isEmpty() || !QFileInfo::exists(m_image1Path)) {
            *error = "Choose image 1.";
            return false;
        }
        if (m_image2Path.isEmpty() || !QFileInfo::exists(m_image2Path)) {
            *error = "Choose image 2.";
            return false;
        }
        if (m_outputEdit->text().trimmed().isEmpty()) {
            *error = "Choose an output MP4 path.";
            return false;
        }
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
            *error = "FFmpeg was not found in PATH. Install it with: sudo pacman -S ffmpeg";
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
        const QString common = QString("scale=%1:%2:force_original_aspect_ratio=decrease,"
                                       "pad=%1:%2:(ow-iw)/2:(oh-ih)/2,"
                                       "setsar=1,fps=%3,format=rgba")
                                   .arg(w).arg(h).arg(fps);

        return QString("[0:v]%1,trim=duration=%2,setpts=PTS-STARTPTS[v0];"
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
        quoted << "ffmpeg";
        for (const QString &arg : ffmpegArgs())
            quoted << shellQuote(arg);
        return quoted.join(' ');
    }

    void render()
    {
        QString error;
        if (!validateInputs(&error)) {
            QMessageBox::warning(this, "Cannot render", error);
            return;
        }

        const QString out = m_outputEdit->text().trimmed();
        QDir().mkpath(QFileInfo(out).absolutePath());

        m_log->appendPlainText("Running:\n" + commandLine() + "\n");
        setRenderingUi(true);

        m_process.reset(new QProcess(this));
        m_process->setProgram("ffmpeg");
        m_process->setArguments(ffmpegArgs());
        m_process->setProcessChannelMode(QProcess::MergedChannels);

        connect(m_process.get(), &QProcess::readyReadStandardOutput, this, [this] {
            m_log->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
        });

        connect(m_process.get(), &QProcess::finished, this, [this, out](int exitCode, QProcess::ExitStatus status) {
            setRenderingUi(false);
            if (status == QProcess::NormalExit && exitCode == 0) {
                m_log->appendPlainText("\nDone: " + out);
                QMessageBox::information(this, "Render complete", "Video created:\n" + out);
            } else {
                m_log->appendPlainText(QString("\nFFmpeg failed. Exit code: %1").arg(exitCode));
                QMessageBox::critical(this, "Render failed", "FFmpeg failed. See the log for details.");
            }
            m_process.reset();
        });

        connect(m_process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            setRenderingUi(false);
            m_log->appendPlainText(QString("\nProcess error: %1").arg(static_cast<int>(e)));
        });

        m_process->start();
    }

    void cancelRender()
    {
        if (!m_process)
            return;
        m_log->appendPlainText("\nCancelling…");
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }

    void copyCommandLine()
    {
        QString error;
        if (!validateInputs(&error)) {
            QMessageBox::warning(this, "Cannot copy command", error);
            return;
        }
        QApplication::clipboard()->setText(commandLine());
        m_log->appendPlainText("Copied FFmpeg command to clipboard.\n");
    }

    void setRenderingUi(bool rendering)
    {
        m_renderButton->setEnabled(!rendering);
        m_cancelButton->setEnabled(rendering);
        m_progress->setVisible(rendering);
    }

private:
    QString m_image1Path;
    QString m_image2Path;

    DropImageLabel *m_image1Label = nullptr;
    DropImageLabel *m_image2Label = nullptr;
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
    QApplication::setApplicationName("Image Transient");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("Remisa");

    MainWindow window;
    window.show();
    return app.exec();
}
