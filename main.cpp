#include <QtWidgets>
#include <QApplication>
#include <QCompleter>
#include <QDateTime>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>


// --- Progress Dialog ---
class InstallProgressDialog : public QDialog {
    Q_OBJECT
public:
    QLabel *infoLabel;
    InstallProgressDialog(const QString &title, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setModal(true);
        setFixedSize(420, 100);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        auto layout = new QVBoxLayout(this);
        infoLabel = new QLabel("Running command...", this);
        layout->addWidget(infoLabel);
        setStyleSheet(
            "QDialog { background: #000; font-family: 'Fira Mono', 'DejaVu Sans Mono', 'monospace'; color: #fff; }"
            "QLabel { font-family: 'Fira Mono', 'DejaVu Sans Mono', 'monospace'; color: #fff; }"
            );
    }
    void showInfo(const QString &msg) { infoLabel->setText(msg); }
};

// --- Terminal Helper ---
QString findTerminal() {
    QStringList terms = {"xterm", "konsole", "qterminal"};
    for (const QString &t : terms) {
        if (!QStandardPaths::findExecutable(t).isEmpty())
            return t;
    }
    return QString();
}
QStringList supportedTerminals() {
    QStringList found;
    QStringList terms = {"xterm", "konsole", "qterminal"};
    for (const QString &t : terms) {
        if (!QStandardPaths::findExecutable(t).isEmpty())
            found << t;
    }
    return found;
}
void runSudoInTerminal(const QString &cmd, QWidget *parent, const QString &desc = QString()) {
    QString terminal = findTerminal();
    auto dlg = new InstallProgressDialog(desc.isEmpty() ? "Running Command..." : desc, parent);
    if (terminal.isEmpty()) {
        dlg->showInfo("No supported terminal emulator found!\nSupported: zutty, xterm, konsole, qterminal\n install one");
        dlg->show();
        QTimer::singleShot(3500, dlg, &QDialog::accept);
        return;
    }
    QStringList args;
    QString sudoCmd = QString("sudo %1; echo; echo '[Press Enter to close]'; read").arg(cmd);
    if (terminal == "xterm")
        args << "-e" << sudoCmd;
    else if (terminal == "konsole")
        args << "-e" << "bash" << "-c" << sudoCmd;
    else if (terminal == "qterminal")
        args << "-e" << "bash" << "-c" << sudoCmd;
    else
        args << "-e" << sudoCmd; // fallback

    QProcess::startDetached(terminal, args);
    dlg->showInfo(desc + "\n Yes, a terminal will open for authentication.");
    dlg->show();
    QTimer::singleShot(2000, dlg, &QDialog::accept);
}



// --- Utility: System Info ---

QString getOSInfo() {
    QFile f("/etc/os-release");
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("PRETTY_NAME="))
                return line.section('=', 1).remove('"');
        }
    }
    return QSysInfo::prettyProductName();
}

QString getKernel() {
    return QSysInfo::kernelType() + " " + QSysInfo::kernelVersion();
}

QString getCPUArch() {
    return QSysInfo::currentCpuArchitecture();
}

QString getCPUModel() {
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            QString line = f.readLine();
            if (line.toLower().startsWith("model name"))
                return line.section(':',1).trimmed();
        }
    }
    QProcess p;
    p.start("lscpu");
    p.waitForFinished(600);
    for (const QString &line : QString(p.readAllStandardOutput()).split('\n')) {
        if (line.toLower().startsWith("model name"))
            return line.section(':',1).trimmed();
    }
    return "Unknown";
}

QString getCPUCoreCount() {
    int cores = 0;
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            QString line = f.readLine();
            if (line.toLower().startsWith("processor")) ++cores;
        }
    }
    if (cores > 0) return QString::number(cores);
    QProcess p;
    p.start("nproc");
    p.waitForFinished(300);
    QString out = QString(p.readAllStandardOutput()).trimmed();
    if (!out.isEmpty()) return out;
    return "Unknown";
}

QString getPhysicalCoreCount() {
    QProcess p;
    p.start("lscpu");
    p.waitForFinished(500);
    QString out = QString(p.readAllStandardOutput());
    QRegularExpression reCores("Core\\(s\\) per socket:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reSockets("Socket\\(s\\):\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m1 = reCores.match(out);
    QRegularExpressionMatch m2 = reSockets.match(out);
    if (m1.hasMatch() && m2.hasMatch()) {
        int a = m1.captured(1).toInt();
        int b = m2.captured(1).toInt();
        return QString::number(a * b);
    }
    QRegularExpression reCpu("^CPU\\(s\\):\\s*(\\d+)$", QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    QRegularExpressionMatch m3 = reCpu.match(out);
    if (m3.hasMatch()) return m3.captured(1);
    return "Unknown";
}

QString getStorage() {
    QStorageInfo s = QStorageInfo::root();
    if (s.isValid() && s.isReady()) {
        quint64 total = s.bytesTotal();
        quint64 avail = s.bytesAvailable();
        quint64 used = (total > avail) ? (total - avail) : 0;
        auto human = [](qulonglong bytes)->QString {
            double b = bytes;
            const char *units[] = {"B","KB","MB","GB","TB"};
            int i=0;
            while (b >= 1024.0 && i<4) { b /= 1024.0; ++i; }
            return QString::number(b, 'f', (i==0?0:1)) + " " + units[i];
        };
        QString pct = total ? QString::number(100.0 * used / (double)total, 'f', 0) : "??";
        return QString("%1 / %2 (%3%)").arg(human(used), human(total), pct);
    }
    // fallback to 'df -h /' parse
    QProcess p;
    p.start("df", {"-h", "/"});
    p.waitForFinished(400);
    QString out = QString(p.readAllStandardOutput());
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("/")) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 5) return QString("%1 used / %2 total (%3)").arg(parts[2], parts[1], parts[4]);
        }
    }
    return "Unknown";
}


QString getHostname() {
    return QSysInfo::machineHostName();
}

QString getUptime() {
    QFile f("/proc/uptime");
    if (f.open(QIODevice::ReadOnly)) {
        QString time = f.readLine().split(' ').first();  // <-- fixed here
        bool ok;
        double seconds = time.toDouble(&ok);
        if (ok) {
            int days = seconds / 86400;
            seconds -= days * 86400;
            int hours = seconds / 3600;
            seconds -= hours * 3600;
            int mins = seconds / 60;
            return QString("%1d %2h %3m").arg(days).arg(hours).arg(mins);
        }
    }
    return "Unknown";
}
QString getRam() {
    QProcess process;
    process.start("grep", {"MemTotal", "/proc/meminfo"});
    process.waitForFinished();

    QString output = process.readAllStandardOutput().trimmed();
    QRegularExpression regex(R"(MemTotal:\s+(\d+)\s+kB)");
    QRegularExpressionMatch match = regex.match(output);

    if (match.hasMatch()) {
        qint64 kb = match.captured(1).toLongLong();
        double gb = kb / 1024.0 / 1024.0;
        return QString::number(gb, 'f', 2) + " GB";
    }
    return "Unknown";
}

QString getInstallDate() {
    QProcess proc;
    proc.start("stat", {"/"});
    proc.waitForFinished();
    QString output = proc.readAllStandardOutput();

    for (const QString &line : output.split("\n")) {
        if (line.contains("Birth")) {
            QString raw = line.section("Birth:", 1).trimmed();

            // Remove fractional seconds and timezone
            QRegularExpression cleanTime(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}))");
            QRegularExpressionMatch match = cleanTime.match(raw);
            if (match.hasMatch()) {
                QString cleaned = match.captured(1);
                QDateTime dt = QDateTime::fromString(cleaned, "yyyy-MM-dd hh:mm:ss");
                return dt.toString("dd MMM yyyy, hh:mm:ss");
            }

            return raw; // fallback
        }
    }

    return "Install date not available";
}

QString getUser() {
    return qEnvironmentVariable("USER", "Unknown");
}

QString getHomePath() {
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString getDocumentsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

QString getDownloadsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

// --- Glowing Clickable Logo ---
class GlowingLogo : public QLabel {
    Q_OBJECT
public:
    GlowingLogo(QWidget *parent = nullptr) : QLabel(parent), clickCount(0) {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    void enterEvent(QEnterEvent *event) override {
#else
    void enterEvent(QEvent *event) override {
#endif
        auto glow = new QGraphicsDropShadowEffect;
        glow->setBlurRadius(25);
        glow->setColor(QColor("#00BFFF"));
        glow->setOffset(0);
        setGraphicsEffect(glow);
        QLabel::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        setGraphicsEffect(nullptr);
        QLabel::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            clickCount++;
            if (clickCount >= 3) {
                emit triggerMiniGame();
                clickCount = 0;
            }
        }
        QLabel::mousePressEvent(event);
    }

signals:
    void triggerMiniGame();

private:
    int clickCount;
};

// --- MiniGame Dialog ---
class MiniGameDialog : public QDialog {
    Q_OBJECT
public:
    MiniGameDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("🌠 Neospace 2025 runner");
        resize(700, 260);
        setStyleSheet("background-color: #111; color: #ddd;");

        jumpCountLabel = new QLabel("Jumps: 0", this);
        jumpCountLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-family: 'DejaVu Sans Mono';");
        jumpCountLabel->setGeometry(10, 8, 120, 20);

        powerupStatusLabel = new QLabel("Powerups: 0", this);
        powerupStatusLabel->setStyleSheet("color: #ff8888; font-family: 'DejaVu Sans Mono';");
        powerupStatusLabel->setGeometry(140, 8, 220, 20);

        player = new QLabel(this);
        QPixmap pp(":/error.os.svgz");
        if (pp.isNull()) pp = QPixmap(32,32);
        player->setPixmap(pp.scaled(playerWidth, playerHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        player->setGeometry(50, groundY - playerHeight, playerWidth, playerHeight);
        playerY = groundY - playerHeight;

        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &MiniGameDialog::gameLoop);
        gameTimer->start(gameIntervalMs);

        spawnTimer = new QTimer(this);
        connect(spawnTimer, &QTimer::timeout, this, &MiniGameDialog::spawnObstacle);
        spawnTimer->start(spawnIntervalMs);

        powerupSpawnTimer = new QTimer(this);
        connect(powerupSpawnTimer, &QTimer::timeout, this, &MiniGameDialog::spawnPowerup);
        powerupSpawnTimer->start(powerupSpawnIntervalMs + QRandomGenerator::global()->bounded(powerupSpawnJitterMs));

        setFocusPolicy(Qt::StrongFocus);
    }

    ~MiniGameDialog() override {
        qDeleteAll(obstacles);
        obstacles.clear();
        if (powerup) powerup->deleteLater();
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if ((e->key() == Qt::Key_Space) && !jumping) doJump();
        QDialog::keyPressEvent(e);
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (!jumping) doJump();
        QDialog::mousePressEvent(e);
    }

private slots:
    void gameLoop() {
        if (jumping) {
            playerY += velocity;
            velocity += gravity;
            if (playerY >= groundY - playerHeight) {
                playerY = groundY - playerHeight;
                jumping = false;
                velocity = 0;
            }
            player->move(player->x(), playerY);
        }

        for (int i = obstacles.size() - 1; i >= 0; --i) {
            QLabel *obs = obstacles.at(i);
            obs->move(obs->x() - obstacleSpeed, obs->y());
            if (obs->x() + obs->width() < 0) {
                obs->deleteLater();
                obstacles.removeAt(i);
            } else if (player->geometry().intersects(obs->geometry())) {
                endGame();
                return;
            }
        }

        if (powerup) {
            powerup->move(powerup->x() - obstacleSpeed, powerup->y());
            if (powerup->x() + powerup->width() < 0) {
                powerup->deleteLater();
                powerup = nullptr;
            } else if (player->geometry().intersects(powerup->geometry())) {
                collectPowerup();
            }
        }
    }

    void spawnObstacle() {
        QLabel *obs = new QLabel(this);
        obs->setStyleSheet("background: #d9534f; border-radius:3px;");
        obs->setGeometry(width(), groundY - obstacleHeight, obstacleWidth, obstacleHeight);                                                 //ayoo very red
        obs->show();
        obstacles.append(obs);
        spawnTimer->start(spawnIntervalMs + QRandomGenerator::global()->bounded(spawnIntervalJitterMs));
    }

    void spawnPowerup() {
        if (powerup) return;

        // pick an emoji from a small set
        QStringList emojis = {
            QString::fromUtf8("💠"),
            QString::fromUtf8("💣"),
            QString::fromUtf8("🥭"),
            QString::fromUtf8("🥚"),
            QString::fromUtf8("🗿"),
            QString::fromUtf8("🧨")
        };
        int idx = QRandomGenerator::global()->bounded(emojis.size());
        QString emoji = emojis.at(idx);

        const int emSize = 32;
        QPixmap pix(emSize, emSize);
        pix.fill(Qt::transparent);

        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        QFont font;
        font.setPointSize(int(emSize * 0.8));
        font.setBold(false);

        // prefer a color emoji font if available
        font.setFamily(QString::fromUtf8("Noto Color Emoji"));
        if (!QFontInfo(font).family().contains("Noto") && QFontInfo(font).pointSize() == 0) {
            font.setFamily(QString()); // fall back to default font
        }

        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(pix.rect(), Qt::AlignCenter, emoji);
        p.end();

        powerup = new QLabel(this);
        powerup->setPixmap(pix);
        powerup->setGeometry(width() - emSize - 10, groundY - playerHeight - emSize - 4, emSize, emSize);
        powerup->setToolTip(QStringLiteral("Powerup: %1").arg(emoji));
        powerup->show();
    }

    void collectPowerup() {
        if (!powerup) return;
        powerup->deleteLater();
        powerup = nullptr;
        applyPermanentSpeedBoost();
    }

    void applyPermanentSpeedBoost() {
        ++permanentPowerups;
        obstacleSpeed = int(obstacleSpeed * speedBoostMultiplier);
        // Optionally cap the speed to avoid runaway values
        if (obstacleSpeed > maxObstacleSpeed) obstacleSpeed = maxObstacleSpeed;
        powerupStatusLabel->setText(QString("Powerups: %1").arg(permanentPowerups));
        powerupStatusLabel->setStyleSheet("color: #88ff88; font-weight: bold; font-family: 'DejaVu Sans Mono';");
    }
    void removeAllObstacles() {
        QList<QLabel*> old;
        old.swap(obstacles);   // YO SUP
        for (QLabel *l : old)
            if (l) l->deleteLater();
    }
    void endGame() {
        gameTimer->stop();
        spawnTimer->stop();
        powerupSpawnTimer->stop();

        // pool of sarcastic messages
        QStringList messages = {
            "You failed spectacularly! Total jumps: %1",
            "Well… that was short-lived. Jumps: %1",
            "Gravity says hi. You managed %1 jumps.",
            "Epic fail unlocked! Score: %1",
            "Ouch. Only %1 jumps before disaster.",
            "Congratulations, you’ve invented a new way to lose. Jumps: %1"
        };

        // pick one at random
        int index = QRandomGenerator::global()->bounded(messages.size());
        QString message = messages.at(index).arg(jumpCount);

        QMessageBox::information(this, "Game Over", message);

        removeAllObstacles();
        if (powerup) { powerup->deleteLater(); powerup = nullptr; }
        accept();
    }


private:
    void doJump() {
        jumping = true;
        velocity = initialJumpVelocity;
        ++jumpCount;
        jumpCountLabel->setText(QString("Jumps: %1").arg(jumpCount));
    }

private:
    QLabel *player = nullptr;
    QList<QLabel*> obstacles;
    QLabel *powerup = nullptr;

    QLabel *jumpCountLabel = nullptr;
    QLabel *powerupStatusLabel = nullptr;

    QTimer *gameTimer = nullptr;
    QTimer *spawnTimer = nullptr;
    QTimer *powerupSpawnTimer = nullptr;

    const int groundY = 200;
    int playerY = groundY - 32;
    const int playerHeight = 32;
    const int playerWidth = 32;
    int velocity = 0;
    bool jumping = false;

    int jumpCount = 0;

    int obstacleWidth = 20;
    int obstacleHeight = 20;
    int obstacleSpeed = 5;
    const int maxObstacleSpeed = 40;
    const int gameIntervalMs = 30;
    const int spawnIntervalMs = 1400;
    const int spawnIntervalJitterMs = 800;

    const int gravity = 1;
    const int initialJumpVelocity = -12;

    const int powerupSpawnIntervalMs = 9000;
    const int powerupSpawnJitterMs = 6000;
    const double speedBoostMultiplier = 1.5;

    int permanentPowerups = 0;
};


// --- System Info Panel ---
class SystemInfoPanel : public QWidget {
    Q_OBJECT
public:
    SystemInfoPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto mainLayout = new QHBoxLayout(this);

        // --- Left side (system info text) ---
        auto leftLayout = new QVBoxLayout;

        QString fullOS = getOSInfo();
        QString shortOS = fullOS.split(' ').first();

        auto osTitle = new QLabel(shortOS);
        osTitle->setStyleSheet("font-size: 28px; font-weight: bold; color: #dfe2ec; margin-bottom: 10px;");
        leftLayout->addWidget(osTitle);

        auto versionLabel = new QLabel(fullOS);
        versionLabel->setStyleSheet("font-size: 14px; color: gray; margin-bottom: 15px;");
        leftLayout->addWidget(versionLabel);

        auto infoBox = new QGroupBox;
        infoBox->setStyleSheet("QGroupBox { border: 1px solid #444; margin-top: 0; }");
        auto infoLayout = new QVBoxLayout(infoBox);

        // --- Expanded system info labels ---
        auto cpuArchLabel    = new QLabel("CPU Arch: " + getCPUArch());
        auto cpuModelLabel   = new QLabel("CPU Model: " + getCPUModel());
        auto cpuCoresLabel   = new QLabel("CPU Cores: " + getCPUCoreCount());
        auto ramLabel        = new QLabel("RAM: " + getRam());
        auto storageLabel    = new QLabel("Storage: " + getStorage());
        auto hostLabel       = new QLabel("Hostname: " + getHostname());
        auto uptimeLabel     = new QLabel("Uptime: " + getUptime());
        auto kernelLabel     = new QLabel("Kernel: " + getKernel());
        auto userLabel       = new QLabel("User: " + getUser());
        auto homeLabel       = new QLabel("Home: " + getHomePath());
        auto docsLabel       = new QLabel("Documents: " + getDocumentsPath());
        auto downloadsLabel  = new QLabel("Downloads: " + getDownloadsPath());
        auto timeLabel       = new QLabel("Time: " + QDateTime::currentDateTime().toString());
        auto installLabel    = new QLabel("error.os install date: " + getInstallDate());

        QList<QLabel*> labels = {
            kernelLabel, cpuArchLabel, cpuModelLabel, cpuCoresLabel,
            ramLabel, storageLabel, hostLabel, uptimeLabel, userLabel, homeLabel, docsLabel,
            downloadsLabel, timeLabel , installLabel
        };

        for (auto l : labels) {
            l->setStyleSheet("color: white; font-size: 13px; margin: 4px 0; font-family: 'Fira Mono', monospace;");
            infoLayout->addWidget(l);
        }

        leftLayout->addWidget(infoBox);
        leftLayout->addStretch();

        // --- Right side (placeholder for logo/game if you want) ---
        auto rightLayout = new QVBoxLayout;
        rightLayout->setAlignment(Qt::AlignCenter);

        // Example: you can add your GlowingLogo widget here
        // auto logo = new GlowingLogo(":/error.os.svgz");
        // rightLayout->addWidget(logo);

        mainLayout->addLayout(leftLayout, 1);
        mainLayout->addLayout(rightLayout, 1);

        // --- Right side (logo + glow + game) ---
        auto iconLabel = new GlowingLogo;
        auto pix = QPixmap(":/error.os.svgz").scaled(355,440, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(pix);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("background: transparent;");
        connect(iconLabel, &GlowingLogo::triggerMiniGame, this, &SystemInfoPanel::launchMiniGame);
        rightLayout->addWidget(iconLabel);
        rightLayout->addStretch();

        // --- Combine ---
        mainLayout->addLayout(leftLayout, 1);
        mainLayout->addLayout(rightLayout, 1);
    }

private slots:
    void launchMiniGame() {
        MiniGameDialog game(this);
        game.exec();
    }
};
// --- Driver Manager Panel ---
class DriverManager : public QWidget {
    Q_OBJECT
public:
    DriverManager(QWidget *parent = nullptr) : QWidget(parent) {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(20);

        auto titleLabel = new QLabel("Driver Manager");
        titleLabel->setStyleSheet(
            "font-size: 32px; font-weight: bold; color: #dfe2ec; margin-bottom: 10px;"
            "font-family: 'DejaVu Sans Mono', monospace;"
            );
        statusLabel = new QLabel("Detecting hardware...");
        statusLabel->setStyleSheet(
            "color: #cccccc; margin-bottom: 20px; font-size: 20px;"
            "font-family: 'DejaVu Sans Mono', monospace;"
            );

        // NVIDIA group
        auto nvidiaGroup = new QGroupBox("🔰  NVIDIA Graphics");
        nvidiaGroup->setStyleSheet(groupBoxStyle());
        auto nvidiaLayout = new QVBoxLayout(nvidiaGroup);
        installNvidiaBtn = styledButton("Install NVIDIA Driver");
        nvidiaLayout->addWidget(installNvidiaBtn);

        // Printer group
        auto printerGroup = new QGroupBox("🖨️  Printer Support");
        printerGroup->setStyleSheet(groupBoxStyle());
        auto printerLayout = new QVBoxLayout(printerGroup);
        installPrinterBtn = styledButton("Install Printer Drivers");
        printerLayout->addWidget(installPrinterBtn);

        // Removal group
        removalGroup = new QGroupBox("🗑️  Remove Unused Drivers (not recommended)");
        removalGroup->setStyleSheet(groupBoxStyle());
        removalLayout = new QVBoxLayout(removalGroup);
        removalGroup->setLayout(removalLayout);

        layout->addWidget(titleLabel);
        layout->addWidget(statusLabel);
        layout->addWidget(nvidiaGroup);
        layout->addWidget(printerGroup);
        layout->addWidget(removalGroup);
        layout->addStretch();

        // Connections
        connect(installNvidiaBtn, &QPushButton::clicked, this, &DriverManager::installNvidiaDriver);
        connect(installPrinterBtn, &QPushButton::clicked, this, &DriverManager::installPrinterDrivers);

        detectHardware();
    }

private slots:
    void detectHardware() {
        QString gpuVendor, cpuVendor;

        // Detect GPU vendor
        {
            QProcess p;
            p.start("sh", {"-c", "lspci | grep -i 'vga\\|3d' "});
            p.waitForFinished();
            QString out = p.readAllStandardOutput().toLower();
            if (out.contains("nvidia")) gpuVendor = "nvidia";
            else if (out.contains("amd") || out.contains("ati")) gpuVendor = "amd";
            else if (out.contains("intel")) gpuVendor = "intel";
            else gpuVendor = "unknown";
        }

        // Detect CPU vendor
        {
            QProcess p;
            p.start("sh", {"-c", "lscpu | grep 'Vendor ID'"});
            p.waitForFinished();
            QString out = p.readAllStandardOutput().toLower();
            if (out.contains("authenticamd")) cpuVendor = "amd";
            else if (out.contains("genuineintel")) cpuVendor = "intel";
            else cpuVendor = "unknown";
        }

        statusLabel->setText(QString("Detected GPU: %1 | CPU: %2").arg(gpuVendor, cpuVendor));

        // Build removal buttons based on detection
        if (gpuVendor == "nvidia") {
            addRemovalButton("Remove AMD/Intel GPU drivers",
                             {"xserver-xorg-video-amdgpu", "xserver-xorg-video-intel"});
        } else if (gpuVendor == "amd") {
            addRemovalButton("Remove NVIDIA/Intel GPU drivers",
                             {"nvidia-driver", "nvidia-settings", "xserver-xorg-video-intel"});
        } else if (gpuVendor == "intel") {
            addRemovalButton("Remove NVIDIA/AMD GPU drivers",
                             {"nvidia-driver", "nvidia-settings", "xserver-xorg-video-amdgpu"});
        }

        if (cpuVendor == "intel") {
            addRemovalButton("Remove AMD microcode", {"amd64-microcode"});
        } else if (cpuVendor == "amd") {
            addRemovalButton("Remove Intel microcode", {"intel-microcode"});
        }
    }

    void installNvidiaDriver() {
        runSudoInTerminal("apt install -y nvidia-driver nvidia-settings", this,
                          "Installing NVIDIA driver...");
    }

    void installPrinterDrivers() {
        runSudoInTerminal("sh -c 'apt update && apt upgrade -y && "
                          "apt install -y cups cups-filters cups-bsd cups-client "
                          "print-manager ipp-usb printer-driver-all && "
                          "systemctl enable cups && systemctl start cups && "
                          "usermod -aG lpadmin $USER && xdg-open http://localhost:631'",
                          this, "Installing printer drivers...");
    }

    void confirmAndRemove(const QStringList &pkgs) {
        QString msg = "The following packages will be removed:\n\n" + pkgs.join("\n") + "\n\nContinue?";
        if (QMessageBox::question(this, "Confirm Removal", msg) == QMessageBox::Yes) {
            QString cmd = "sh -c 'apt purge -y " + pkgs.join(" ") + " || true'";
            runSudoInTerminal(cmd, this, "Removing unused drivers...");
        }
    }

private:
    void addRemovalButton(const QString &label, const QStringList &pkgs) {
        auto *btn = styledButton(label);
        removalLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, pkgs]() { confirmAndRemove(pkgs); });
    }

    QString groupBoxStyle() const {
        return "QGroupBox { color: #ffffff; font-weight: bold; font-size: 14px; margin-top: 15px;"
               "padding-top: 15px; background: #0a0a0a; border: 1px solid #1a3cff; border-radius: 8px;"
               "font-family: 'DejaVu Sans Mono', monospace; }"
               "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 10px; color: #00bfff; }";
    }

    QPushButton* styledButton(const QString &text) {
        auto *btn = new QPushButton(text);
        btn->setStyleSheet("QPushButton { background: #1a3cff; color: #ffffff; border: none;"
                           "padding: 12px 20px; border-radius: 6px; font-family: 'DejaVu Sans Mono', monospace;"
                           "font-weight: bold; font-size: 12px; }"
                           "QPushButton:hover { background: #2a4cff; }"
                           "QPushButton:pressed { background: #0a2cff; }");
        return btn;
    }

    QLabel *statusLabel;
    QPushButton *installNvidiaBtn;
    QPushButton *installPrinterBtn;
    QGroupBox *removalGroup;
    QVBoxLayout *removalLayout;
};

// --- App Installer Panel ---
class AppInstaller : public QWidget {
    Q_OBJECT
public:
    AppInstaller(QWidget *parent = nullptr) : QWidget(parent) {
        auto layout = new QVBoxLayout(this);

        auto titleLabel = new QLabel("Application Installer");
        titleLabel->setStyleSheet("font-size: 32px; font-weight: bold; color:#dfe2ec ; margin: 20px 0;");
        layout->addWidget(titleLabel);
        auto helpLabel = new QLabel("Check  boxes and install");
        helpLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00BFFF; margin: 2px 0;");
        layout->addWidget(helpLabel);

        tabs = new QTabWidget;
        tabs->addTab(createDpkgTab(), "dpkg");
        tabs->addTab(createFlatpakTab(), "Flatpak");
        tabs->addTab(createWgetTab(), "wget");
        layout->addWidget(tabs);
    }

private:
    QTabWidget *tabs;

    // --- DPKG/APT Tab ---
    QWidget* createDpkgTab() {
        auto widget = new QWidget;
        auto vbox = new QVBoxLayout(widget);

        auto intro = new QLabel("dpkg is the low-level tool that installs, removes, and manages .deb packages on Debian-based systems it's what APT uses behind the scenes to do the actual work.");
        intro->setWordWrap(true);
        intro->setStyleSheet("font-size: 10px; color: #dfe2ec; font-weight: bold; margin-bottom: 1px; font-family: 'Monospace';");
        intro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        vbox->addWidget(intro);


        auto status = new QLabel("install on yoour error.os system.");
        status->setStyleSheet("color: white; font-family: 'Fira Mono';");
        vbox->addWidget(status);

        auto list = new QListWidget;
        list->setStyleSheet("QListWidget { background:#111; color:white; border:1px solid #223355; }");
        for (const auto &app : dpkgApps) {
            auto item = new QListWidgetItem(QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.package);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        auto btn = new QPushButton("Install Selected (dpkg Only)");
        btn->setStyleSheet("QPushButton { background:#112266; color:white; padding:8px; }");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [=, this]() {
            QStringList pkgs;
            for (int i=0;i<list->count();++i) {
                auto item = list->item(i);
                if (item->checkState()==Qt::Checked)
                    pkgs << item->data(Qt::UserRole).toString();
            }
            if (pkgs.isEmpty()) { status->setText("No apps selected."); return; }
            status->setText("Installing via APT...");
            runSudoInTerminal("apt install -y " + pkgs.join(' '), this, "Installing apps...");
        });

        return widget;
    }

    // --- Flatpak Tab ---
    QWidget* createFlatpakTab() {
        auto widget = new QWidget;
        auto vbox = new QVBoxLayout(widget);

        auto intro = new QLabel("Flatpak is a universal app system for Linux that lets you install sandboxed software from anywhere—without worrying about your distro’s package manager.");
        intro->setWordWrap(true);
        intro->setStyleSheet("font-size: 10px; color: #dfe2ec; font-weight: bold; margin-bottom: 1px; font-family: 'Monospace';");
        intro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        vbox->addWidget(intro);

        auto status = new QLabel("Select to install on flatpak");
        status->setStyleSheet("color: white; font-family: 'Fira Mono';");
        vbox->addWidget(status);

        auto list = new QListWidget;
        list->setStyleSheet("QListWidget { background:#111; color:white; border:1px solid #223355; }");
        for (const auto &app : flatpakApps) {
            auto item = new QListWidgetItem(QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.package);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        auto btn = new QPushButton("Install Selected (Flatpak only)");
        btn->setStyleSheet("QPushButton { background:#112266; color:white; padding:8px; }");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [=, this]() {
            QStringList pkgs;
            for (int i=0;i<list->count();++i) {
                auto item = list->item(i);
                if (item->checkState()==Qt::Checked)
                    pkgs << item->data(Qt::UserRole).toString();
            }
            if (pkgs.isEmpty()) { status->setText("No apps selected."); return; }

            QProcess check;
            check.start("which", {"flatpak"});
            check.waitForFinished();
            bool hasFlatpak = !check.readAllStandardOutput().trimmed().isEmpty();
            if (!hasFlatpak) {
                status->setText("Flatpak not found. Installing...");
                runSudoInTerminal("apt update && apt install -y flatpak plasma-discover-backend-flatpak && "
                                  "flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo && "
                                  "flatpak update -y", this, "Installing Flatpak...");
            }

            QString cmd = "flatpak install -y " + pkgs.join(' ');
            status->setText("Installing selected Flatpak apps...");
            runSudoInTerminal(cmd, this, "Installing Flatpak apps...");
        });

        return widget;
    }

    // --- Wget Tab ---
    QWidget* createWgetTab() {
        auto widget = new QWidget;
        auto vbox = new QVBoxLayout(widget);

        auto intro = new QLabel("wget installer fetches official .deb packages directly from vendor sites and installs them automatically—no repo required, just raw speed.");
        intro->setWordWrap(true);
        intro->setStyleSheet("font-size: 10px; color: #dfe2ec; font-weight: bold; margin-bottom: 1px; font-family: 'Monospace';");
        intro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        vbox->addWidget(intro);

        auto status = new QLabel("Select applications to install ( ⚠️ filesizes are maybe bigger):");
        status->setStyleSheet("color: white; font-family: 'Fira Mono';");
        vbox->addWidget(status);

        auto list = new QListWidget;
        list->setStyleSheet("QListWidget { background:#111; color:white; border:1px solid #223355; }");
        for (const auto &app : wgetApps) {
            auto item = new QListWidgetItem(QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.package);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        auto btn = new QPushButton("Download and Install");
        btn->setStyleSheet("QPushButton { background:#112266; color:white; padding:8px; }");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [=, this]() {
            QStringList cmds;
            for (int i=0;i<list->count();++i) {
                auto item = list->item(i);
                if (item->checkState()==Qt::Checked)
                    cmds << item->data(Qt::UserRole).toString();
            }
            if (cmds.isEmpty()) { status->setText("No apps selected."); return; }

            status->setText("Downloading and installing via wget...");
            for (const auto &cmd : cmds) {
                runSudoInTerminal(cmd, this, "Installing app via wget...");
            }
        });

        return widget;
    }

    // --- App Lists ---
    struct App { QString name, package, description; };

    QList<App> dpkgApps = {
        {"Firefox", "firefox-esr", "Web Browser"},
        {"VLC", "vlc", "Media Player"},
        {"LibreOffice", "libreoffice", "Office Suite"},
        {"GIMP", "gimp", "Image Editor"},
        {"Geany", "geany", "Code Editor"},
        {"Thunderbird", "thunderbird", "Email Client"},
        {"FileZilla", "filezilla", "FTP Client"},
        {"GCompris", "gcompris-qt", "Educational suite with 100+ activities for kids"},
        {"KStars", "kstars", "Astronomy planetarium with star maps and telescope control"},
        {"Celestia", "celestia-gnome", "3D Universe simulator for exploring space"},
        {"Stellarium", "stellarium", "Realistic planetarium with night sky simulation"},
        {"KAlgebra", "kalgebra", "Graphing calculator and math visualization"},
        {"KBruch", "kbruch", "Practice fractions and percentages"},
        {"Kig", "kig", "Interactive geometry learning tool"},
        {"Marble", "marble", "Virtual globe and world atlas"},
        {"TuxMath", "tuxmath", "Math game with Tux shooting comets"},
        {"TuxTyping", "tuxtype", "Typing tutor game with Tux"},
        {"Scratch", "scratch", "Visual programming for kids"},
        {"KTurtle", "kturtle", "Educational programming environment for beginners"},
        {"SuperTux", "supertux", "2D Platformer starring Tux"},
        {"Extreme Tux Racer", "extremetuxracer", "Fast-paced downhill racing with Tux"},
        {"SuperTuxKart", "supertuxkart", "3D Kart Racing with Tux & friends"},
        {"Warmux", "warmux", "Worms-like strategy game with mascots"},
        {"FreedroidRPG", "freedroidrpg", "Sci-fi RPG with Tux"},
        {"Pingus", "pingus", "Lemmings-style puzzle game with penguins"},
        {"Inkscape", "inkscape", "Vector Graphics Editor"},
        {"Krita", "krita", "Digital Painting"},
        {"Pinta", "pinta", "Simple Image Editor"},
        {"Okular", "okular", "PDF & Document Viewer"},
        {"Evince", "evince", "Lightweight PDF Viewer"},
        {"Calibre", "calibre", "E-book Manager"},
        {"Simple Scan", "simple-scan", "Document Scanner"},
        {"Remmina", "remmina", "Remote Desktop Client"},
        {"Audacity", "audacity", "Audio Editor"},
        {"Kdenlive", "kdenlive", "Video Editor"},
        {"OBS Studio", "obs-studio", "Screen Recorder & Streaming"},
        {"Shotwell", "shotwell", "Photo Manager"},
        {"Cheese", "cheese", "Webcam App"},
        {"Guvcview", "guvcview", "Webcam Viewer/Recorder"},
        {"Rhythmbox", "rhythmbox", "Music Player"},
        {"Clementine", "clementine", "Music Player & Library Manager"}
    };

    QList<App> flatpakApps = {
        {"Steam", "com.valvesoftware.Steam", "Gaming Platform"},
        {"Discord", "com.discordapp.Discord", "Chat & Voice"},
        {"Spotify", "com.spotify.Client", "Music Streaming"},
        {"OBS Studio", "com.obsproject.Studio", "Screen Recorder"},
        {"Kdenlive", "org.kde.kdenlive", "Video Editor"},
        {"Audacity", "org.audacityteam.Audacity", "Audio Editor"},
        {"Inkscape", "org.inkscape.Inkscape", "Vector Graphics"},
        {"Blender","org.blender.Blender", "3D Creation Suite"},
        {"Chromium", "org.chromium.Chromium", "Web Browser"},
        {"Telegram", "org.telegram.desktop", "Messaging Client"},
        {"OnlyOffice", "org.onlyoffice.desktopeditors", "Office Suite"},
        {"Remmina", "org.remmina.Remmina", "Remote Desktop"},
        {"Krita", "org.kde.krita", "Digital Painting"},
        {"HandBrake", "fr.handbrake.ghb", "Video Transcoder"}
    };

    // --- wget Apps (direct .deb installers) ---
    QList<App> wgetApps = {
        {
            "WPS Office",
            "wget -O /tmp/wps-office.deb https://wdl1.pcfg.cache.wpscdn.com/wpsdl/wpsoffice/download/linux/11702/wps-office_11.1.0.11702.XA_amd64.deb && sudo dpkg -i /tmp/wps-office.deb || sudo apt -f install -y",
            "Office Suite (from Kingsoft official site)"
        },
        {
            "Visual Studio Code",
            "wget -O /tmp/code.deb https://update.code.visualstudio.com/latest/linux-deb-x64/stable && sudo dpkg -i /tmp/code.deb || sudo apt -f install -y",
            "Code Editor (from Microsoft official site)"
        },
        {
            "Apache OpenOffice",
            "wget -O /tmp/openoffice.tar.gz https://downloads.apache.org/openoffice/4.1.15/binaries/en-US/Apache_OpenOffice_4.1.15_Linux_x86-64_install-deb_en-US.tar.gz && tar -xzf /tmp/openoffice.tar.gz -C /tmp && sudo dpkg -i /tmp/en-US/DEBS/*.deb || sudo apt -f install -y",
            "Office Suite (from Apache official site)"
        },
        {
            "Google Chrome",
            "wget -O /tmp/google-chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb && sudo dpkg -i /tmp/google-chrome.deb || sudo apt -f install -y",
            "Web Browser (from Google official site)"
        },
        {
            "TeamViewer",
            "wget -O /tmp/teamviewer.deb https://download.teamviewer.com/download/linux/teamviewer_amd64.deb && sudo dpkg -i /tmp/teamviewer.deb || sudo apt -f install -y",
            "Remote Support (from TeamViewer official site)"
        },
        {
            "Opera One",
            "wget -O /tmp/opera.deb https://ftp.opera.com/pub/opera/desktop/105.0.4970.21/linux/opera-stable_105.0.4970.21_amd64.deb && sudo dpkg -i /tmp/opera.deb || sudo apt -f install -y",
            "Web Browser (from Opera official site)"
        },
        {
            "Lutris",
            "wget -O /tmp/lutris.deb https://github.com/lutris/lutris/releases/latest/download/lutris_amd64.deb && sudo dpkg -i /tmp/lutris.deb || sudo apt -f install -y",
            "Game Launcher (from Lutris GitHub releases)"
        },
        {
            "Itch.io App",
            "wget -O /tmp/itch.deb https://github.com/itchio/itch/releases/latest/download/itch-setup.deb && sudo dpkg -i /tmp/itch.deb || sudo apt -f install -y",
            "Indie Game Launcher (from Itch.io GitHub)"
        },
        {
            "Canva Desktop",
            "wget -O /tmp/canva.deb https://github.com/canva-desktop/canva-linux/releases/latest/download/canva-desktop_amd64.deb && sudo dpkg -i /tmp/canva.deb || sudo apt -f install -y",
            "Design Tool (community Electron wrapper)"
        },
        {
            "Zoom",
            "wget -O /tmp/zoom.deb https://zoom.us/client/latest/zoom_amd64.deb && sudo dpkg -i /tmp/zoom.deb || sudo apt -f install -y",
            "Video Conferencing (from Zoom official site)"
        },
        {
            "ProtonVPN",
            "wget -O /tmp/protonvpn.deb https://protonvpn.com/download/protonvpn-stable-release.deb && sudo dpkg -i /tmp/protonvpn.deb || sudo apt -f install -y",
            "VPN Client (from ProtonVPN official site)"
        }

    };
};

// APP remover loloooooooooooooooooooooooooooooooooooooooooooo

class AppRemover : public QWidget {
    Q_OBJECT
public:
    explicit AppRemover(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *titleLabel = new QLabel("Application Removal 🗑️");
        titleLabel->setStyleSheet(
            "font-size: 32px; font-weight: bold; color: #dfe2ec; margin: 20px 0;"
            );

        QFrame *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedHeight(2);
        line->setStyleSheet("background-color: #555; margin: 5px 0;");


        disLabel = new QLabel(
            "⚠️  Disclaimer: don't remove applications related to the core system.\n"
            "If you don't know about an application, search the web first."
            );
        disLabel->setWordWrap(true);
        disLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        disLabel->setStyleSheet(
            "color: #bbb; margin-bottom: 10px; "
            "font-family: monospace; font-size: small;"
            );

        statusLabel = new QLabel("Enter application name to remove:");
        statusLabel->setStyleSheet(
            "color: white; margin-bottom: 10px; "
            "font-family: 'Fira Mono','DejaVu Sans Mono',monospace;"
            );

        inputEdit = new QLineEdit();
        inputEdit->setPlaceholderText("Some very new packages may not be suggested");
        inputEdit->setStyleSheet(
            "background-color: #111; color: white; "
            "border: 1px solid #223355; padding: 8px; border-radius: 4px; "
            "font-family: 'Fira Mono','DejaVu Sans Mono',monospace;"
            );

        QStringList installedPkgs = getInstalledPackages();
        QCompleter *completer = new QCompleter(installedPkgs, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        inputEdit->setCompleter(completer);

        removeBtn = new QPushButton("Remove Application");
        removeBtn->setStyleSheet(
            "QPushButton { background-color: #112266; color: white; border: none; "
            "padding: 12px; border-radius: 5px; font-weight: bold; "
            "font-family: 'Fira Mono','DejaVu Sans Mono',monospace; }"
            "QPushButton:hover { background-color: #1a3cff; "
            "box-shadow: 0 0 10px #1a3cff; }"
            );

        layout->addWidget(titleLabel);
        layout->addWidget(line);
        layout->addWidget(disLabel);
        layout->addWidget(statusLabel);
        layout->addWidget(inputEdit);
        layout->addWidget(removeBtn);

        connect(removeBtn, &QPushButton::clicked, this, &AppRemover::removeAppByName);
    }

private:
    QLineEdit   *inputEdit;
    QPushButton *removeBtn;
    QLabel      *statusLabel;
    QLabel      *disLabel;

    QStringList getInstalledPackages() {
        QStringList pkgs;
        QProcess proc;
        proc.start("dpkg-query", {"-f=${Status} ${Package}\\n", "-W"});
        proc.waitForFinished(3000);
        QString out = proc.readAllStandardOutput();

        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            if (line.startsWith("install ok installed")) {
                QString pkg = line.section(' ', 3);
                pkgs << pkg.trimmed();
            }
        }
        return pkgs;
    }

private slots:
    void removeAppByName() {
        QString pkg = inputEdit->text().trimmed();
        if (pkg.isEmpty()) {
            statusLabel->setText("Please enter a package name.");
            return;
        }
        statusLabel->setText("Removing " + pkg + "...");
        removeBtn->setEnabled(false);

        QString cmd = "apt remove -y " + pkg;
        runSudoInTerminal(cmd, this, "Removing application...");
        removeBtn->setEnabled(true);
    }
};


// --- Settings Panel (Launcher) ---
class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    SettingsPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto layout = new QVBoxLayout(this);

        auto iconLabel = new QLabel;
        iconLabel->setPixmap(QPixmap(":/er.svgz").scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignCenter);

        auto titleLabel = new QLabel("err_");
        titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00BFFF; margin: 10px 0;");

        auto infoLabel = new QLabel("version 0.1 (for neospace 2025)");
        infoLabel->setStyleSheet("color: white; margin-bottom: 10px; font-family: 'Fira Mono', 'DejaVu Sans Mono', 'monospace';");

        auto openBtn = new QPushButton(" ⚙️ Open Settings ");
        openBtn->setStyleSheet(
            "QPushButton { background-color: #112266; color: white; border: none; padding: 12px; border-radius: 5px; font-weight: bold; font-family: 'Fira Mono'; }"
            "QPushButton:hover { background-color: #1a3cff; box-shadow: 0 0 10px #1a3cff; }"
            );

        auto githubBtn = new QPushButton("🔗Check in ");
        githubBtn->setStyleSheet(
            "QPushButton { background-color: #222; color: #00BFFF; border: 1px solid #00BFFF; padding: 8px; border-radius: 4px; font-family: 'Fira Mono'; }"
            "QPushButton:hover { background-color: #00BFFF; color: black; }"
            );
        auto wallpBtn = new QPushButton(" 🖼️ Check out more wallpapers for error.os ");
        wallpBtn->setStyleSheet(
            "QPushButton { background-color: #222; color: #00BFFF; border: 1px solid #00BFFF; padding: 8px; border-radius: 4px; font-family: 'Fira Mono'; }"
            "QPushButton:hover { background-color: #00BFFF; color: black; }"
            );
        auto versionLabel = new QLabel("err_ v1.0.3 — error.dashboard for neospace");
        versionLabel->setStyleSheet("color: #AAAAAA; font-size: 12px; font-family: 'Fira Mono'; margin-top: 10px;");
        versionLabel->setAlignment(Qt::AlignCenter);

        auto descLabel = new QLabel("this application is made for troubleshooting error.os's barebones issues");
        descLabel->setStyleSheet("color: #CCCCCC; font-size: 11px; font-family: 'Fira Mono'; margin-bottom: 10px;");
        descLabel->setWordWrap(true);
        descLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(iconLabel);
        layout->addWidget(titleLabel);
        layout->addWidget(infoLabel);
        layout->addWidget(openBtn);
        layout->addWidget(githubBtn);
        layout->addWidget(wallpBtn);
        layout->addWidget(versionLabel);
        layout->addWidget(descLabel);
        layout->addStretch();

        connect(openBtn, &QPushButton::clicked, this, []() {
            if (!QProcess::startDetached("systemsettings"))
                QProcess::startDetached("lxqt-config");
        });

        connect(githubBtn, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/zynomon/err_"));
        });

        connect(wallpBtn, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/zynomon/errpapers"));
        });
    }
};

// --- Main Window ---
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("err_ - Your home");
        setWindowIcon(QIcon(":/err_.svgz"));
        setMinimumSize(800, 540);
        resize(900, 600);

        QWidget *mainWidget = new QWidget(this);
        setCentralWidget(mainWidget);

        QVBoxLayout *layout = new QVBoxLayout(mainWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QTabWidget *tabWidget = new QTabWidget(this);


                   //  Remove if you  want pure qt ui from here to before  tabWidget->add tab  line

        tabWidget->setStyleSheet(
            "QTabWidget::pane { background-color: #000000; border: 1px solid #2a3245; }"
            "QTabBar::tab { background-color: #1a1a1a; color: #dfe2ec; padding: 8px 18px; font-family: 'Fira Mono'; border: none; }"
            "QTabBar::tab:selected { background-color: #5a6fff; color: #000000; font-weight: bold; }"
            "QTabBar::tab:hover { background-color: #6c7fff; color: #ffffff; }"
            );


        tabWidget->addTab(new SystemInfoPanel(), " ℹ️ System Info ");
        tabWidget->addTab(new DriverManager(), " 🖨️ Drivers ");
        tabWidget->addTab(new AppInstaller(), " 📩 Install Apps ");
        tabWidget->addTab(new AppRemover(), " 🗑️ Remove Apps ");
        tabWidget->addTab(new SettingsPanel(), " ⚙️ Settings ");

        layout->addWidget(tabWidget);
    }
};



#include "main.moc"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("err_");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("error.os");

    // Set monospace font globally
    QFont mono("monospace");
    if (!mono.exactMatch()) mono = QFont("DejaVu Sans Mono");
    if (!mono.exactMatch()) mono = QFont("monospace");
    mono.setStyleHint(QFont::Monospace);
    app.setFont(mono);

    MainWindow window;
    window.show();

    return app.exec();
}
