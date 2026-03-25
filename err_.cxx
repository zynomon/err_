#include "err_.H"

class SystemInfoFetcher {
public:
    struct Info {
        QString osName;
        QString osPretty;
        QString kernel;
        QString cpuArch;
        QString cpuModel;
        QString cpuCores;
        QString physicalCores;
        QString ram;
        QString storage;
        QString hostname;
        QString uptime;
        QString user;
        QString homePath;
        QString documentsPath;
        QString downloadsPath;
        QString installDate;
        QString currentTime;
    };

    static Info fetch() {
        Info info;
        info.osPretty = getOSInfo();
        info.osName = getShortOSName(info.osPretty);
        info.kernel = getKernel();
        info.cpuArch = getCPUArch();
        info.cpuModel = getCPUModel();
        info.cpuCores = getCPUCoreCount();
        info.physicalCores = getPhysicalCoreCount();
        info.ram = getRam();
        info.storage = getStorage();
        info.hostname = getHostname();
        info.uptime = getUptime();
        info.user = getUser();
        info.homePath = getHomePath();
        info.documentsPath = getDocumentsPath();
        info.downloadsPath = getDownloadsPath();
        info.installDate = getInstallDate();
        info.currentTime = QDateTime::currentDateTime().toString("dd MMM yyyy HH:mm:ss");
        return info;
    }

private:
    static QString getOSInfo() {
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

    static QString getShortOSName(const QString &fullOS) {
        if (fullOS.contains("<!>")) return "error.os";
        if (fullOS.length() < 10) return fullOS;
        return fullOS.split(' ', Qt::SkipEmptyParts).first();
    }

    static QString getKernel() {
        return QSysInfo::kernelType() + " " + QSysInfo::kernelVersion();
    }

    static QString getCPUArch() {
        return QSysInfo::currentCpuArchitecture();
    }

    static QString getCPUModel() {
        QFile f("/proc/cpuinfo");
        if (f.open(QIODevice::ReadOnly)) {
            while (!f.atEnd()) {
                QString line = f.readLine();
                if (line.toLower().startsWith("model name"))
                    return line.section(':', 1).trimmed();
            }
        }

        QProcess p;
        p.start("lscpu");
        p.waitForFinished(600);
        for (QString line : p.readAllStandardOutput().split('\n')) {
            if (line.toLower().startsWith("model name"))
                return line.section(':', 1).trimmed();
        }
        return "Unknown";
    }

    static QString getCPUCoreCount() {
        if (int cores = QThread::idealThreadCount(); cores > 0)
            return QString::number(cores);

        int count = 0;
        QFile f("/proc/cpuinfo");
        if (f.open(QIODevice::ReadOnly)) {
            while (!f.atEnd()) {
                if (f.readLine().toLower().startsWith("processor")) ++count;
            }
            if (count > 0) return QString::number(count);
        }

        QProcess p;
        p.start("nproc");
        p.waitForFinished(300);
        QString out = p.readAllStandardOutput().trimmed();
        return !out.isEmpty() ? out : "Unknown";
    }

    static QString getPhysicalCoreCount() {
        QFile f("/proc/cpuinfo");
        if (f.open(QIODevice::ReadOnly)) {
            int pkgs = 0, lastPkg = -1;
            while (!f.atEnd()) {
                QString line = f.readLine();
                if (line.toLower().startsWith("physical id")) {
                    int pkg = line.section(':', 1).trimmed().toInt();
                    if (pkg != lastPkg) { pkgs++; lastPkg = pkg; }
                }
            }
            if (pkgs > 0) return QString::number(pkgs);
        }

        QProcess p;
        p.start("lscpu");
        p.waitForFinished(500);
        return "Unknown";
    }

    static QString getRam() {
        QFile f("/proc/meminfo");
        if (!f.open(QIODevice::ReadOnly)) return "Unknown";

        QTextStream in(&f);
        QString line;
        while (in.readLineInto(&line)) {
            if (line.startsWith("MemTotal:")) {
                QString parts = line.section(':', 1).trimmed();
                QString kb = parts.split(' ').first();
                double gb = kb.toDouble() / 1024.0 / 1024.0;
                return QString::number(gb, 'f', 1) + " GB";
            }
        }
        return "Unknown";
    }

    static QString getStorage() {
        QStorageInfo s = QStorageInfo::root();
        if (s.isValid() && s.isReady()) {
            quint64 total = s.bytesTotal();
            quint64 avail = s.bytesAvailable();
            quint64 used = (total > avail) ? (total - avail) : 0;
            auto human = [](qulonglong bytes)->QString {
                double b = bytes;
                const char *units[] = {"B","KB","MB","GB","TB"};
                int i = 0;
                while (b >= 1024.0 && i < 4) { b /= 1024.0; ++i; }
                return QString::number(b, 'f', (i == 0 ? 0 : 1)) + " " + units[i];
            };
            QString pct = total ? QString::number(100.0 * used / (double)total, 'f', 0) : "??";
            return QString("%1 / %2 (%3%)").arg(human(used), human(total), pct);
        }
        return "Unknown";
    }

    static QString getHostname() {
        return QSysInfo::machineHostName();
    }

    static QString getUptime() {
        QFile f("/proc/uptime");
        if (f.open(QIODevice::ReadOnly)) {
            QString time = f.readLine().split(' ').first();
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

    static QString getInstallDate() {
        QFileInfo root("/");
        if (root.lastModified().isValid())
            return root.lastModified().toString("dd MMM yyyy");
        return "Not available";
    }

    static QString getUser() {
        return qEnvironmentVariable("USER", "Unknown");
    }

    static QString getHomePath() {
        return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    static QString getDocumentsPath() {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    static QString getDownloadsPath() {
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
};

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
            "QDialog { background: #000; font-family: 'Nimbus Mono'; color: #fff; }"
            "QLabel { font-family:  'Nimbus Mono'; color: #fff; }"
            );
    }
    void showInfo(const QString &msg) { infoLabel->setText(msg); }
};
QString findTerminal() {
    QStringList terms = {"xterm", "konsole", "qterminal"};
    auto it = std::find_if(terms.begin(), terms.end(), [](const QString &t) {
        return !QStandardPaths::findExecutable(t).isEmpty();
    });
    return it != terms.end() ? *it : QString("");
}
void runInTerminal(const QString &cmd, QWidget *parent, const QString &desc = QString()) {
    QString terminal = findTerminal();
    auto dlg = new InstallProgressDialog(desc.isEmpty() ? "Running Command..." : desc, parent);
    if (terminal.isEmpty()) {
        dlg->showInfo("No supported terminal emulator found!\nSupported: xterm, konsole, qterminal\n install one");
        dlg->show();
        QTimer::singleShot(3500, dlg, &QDialog::accept);
        return;
    }
    QStringList args;
    QString fullCmd = QString("bash -c 'echo \"Running: %1\" && sudo %2; echo; echo \"Press Enter to close\"; read'").arg(cmd, cmd);
    args << "-e" << fullCmd;

    QProcess::startDetached(terminal, args);
    dlg->showInfo(desc + "\nA terminal will open for authentication.");
    dlg->show();
    QTimer::singleShot(2000, dlg, &QDialog::accept);
}



class GlowingLogo : public QLabel {
    Q_OBJECT
public:
    GlowingLogo(QWidget *parent = nullptr) : QLabel(parent), clickCount(0) {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void enterEvent(QEnterEvent *event) override {
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


class MiniGameDialog : public QDialog {
    Q_OBJECT
public:
    MiniGameDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowIcon (QIcon::fromTheme ("<!>"));
        setWindowTitle("Neospace 26 runner");
        resize(700, 260);

        jumpCountLabel = new QLabel("Jumps: 0", this);
        jumpCountLabel->setGeometry(10, 8, 120, 20);

        powerupStatusLabel = new QLabel("Powerups: 0", this);
        powerupStatusLabel->setGeometry(140, 8, 220, 20);

        player = new QLabel(this);
        QPixmap pp;
        QIcon themeIcon = QIcon::fromTheme("error.os");
        if (!themeIcon.isNull()) {
            pp = themeIcon.pixmap(32, 32);
        } else {
            pp = QPixmap(":/error.os.svgz");
            if (pp.isNull()) pp = QPixmap(32, 32);
        }
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
        QStringList emojis = {
            QString::fromUtf8("🧱"),
            QString::fromUtf8("💀"),
            QString::fromUtf8("🌵"),
            QString::fromUtf8("🪨"),
            QString::fromUtf8("🔥"),
            QString::fromUtf8("🌊")
        };
        int idx = QRandomGenerator::global()->bounded(emojis.size());
        obs->setText(emojis.at(idx));
        obs->setAlignment(Qt::AlignCenter);
        obs->setGeometry(width(), groundY - obstacleHeight, obstacleWidth, obstacleHeight);
        obs->show();
        obstacles.append(obs);
        spawnTimer->start(spawnIntervalMs + QRandomGenerator::global()->bounded(spawnIntervalJitterMs));
    }

    void spawnPowerup() {
        if (powerup) return;

        QStringList emojis = {
            QString::fromUtf8("💠"),
            QString::fromUtf8("💣"),     //very coonfusing indeed
            QString::fromUtf8("🥭"),
            QString::fromUtf8("🥚"),
            QString::fromUtf8("🗿"),
            QString::fromUtf8("🧨")
        };
        int idx = QRandomGenerator::global()->bounded(emojis.size());

        powerup = new QLabel(this);
        powerup->setText(emojis.at(idx));
        powerup->setAlignment(Qt::AlignCenter);
        powerup->setGeometry(width() - 42, groundY - playerHeight - 36, 32, 32);
        powerup->setToolTip(QStringLiteral("Powerup: %1").arg(emojis.at(idx)));
        powerup->show();
    }

    void collectPowerup() {
        if (!powerup) return;
        powerup->deleteLater();
        powerup = nullptr;
        ++permanentPowerups;
        obstacleSpeed = int(obstacleSpeed * speedBoostMultiplier);
        if (obstacleSpeed > maxObstacleSpeed) obstacleSpeed = maxObstacleSpeed;
        powerupStatusLabel->setText(QString("Powerups: %1").arg(permanentPowerups));
    }

    void removeAllObstacles() {
        QList<QLabel*> old;
        old.swap(obstacles);
        for (QLabel *l : old)
            if (l) l->deleteLater();
    }

    void endGame() {
        gameTimer->stop();
        spawnTimer->stop();
        powerupSpawnTimer->stop();

        QStringList messages = {
            "You failed spectacularly! Total jumps: %1",
            "Well… that was short-lived. Jumps: %1",
            "Gravity says hi. You managed %1 jumps.",
            "Epic fail unlocked! Score: %1",
            "Ouch. Only %1 jumps before disaster.",
            "Congratulations, you’ve invented a new way to lose. Jumps: %1",
            "Pro tip: Jumping helps. You got %1.",
            "That landed about as gracefully as a sack of bricks. Jumps: %1",
            "New personal worst achieved! %1 jumps.",
            "The ground appreciates your frequent visits. Score: %1",
            "Skill issue detected. Attempts survived: %1",
            "You vs Gravity: Gravity wins again. Jumps: %1",
            "Almost had it… psych! Only %1 jumps.",
            "Achievement unlocked: Faceplant Master. Score: %1",
            "That was less 'jump' and more 'controlled fall'. %1 jumps.",
            "Even the floor is tired of seeing you. Jumps: %1",
            "Bold strategy: straight down. Result: %1 jumps.",
            "Physics: 1, You: 0. Total jumps: %1",
            "Nice try… if trying to lose was the goal. %1 jumps.",
            "You’ve been personally invited to try again. Jumps: %1",
            "World record for shortest run: %1 jumps!",
            "The game thanks you for the entertainment. Score: %1",
            "Plot twist: You were the obstacle all along. %1 jumps.",
            "Error 404: Jumping skills not found. Score: %1"
        };

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
class SystemInfoPanel : public QWidget {
    Q_OBJECT
public:
    SystemInfoPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto mainLayout = new QHBoxLayout(this);

        auto leftLayout = new QVBoxLayout;

        SystemInfoFetcher::Info sysInfo = SystemInfoFetcher::fetch();

        QString shortOS;
        if (sysInfo.osPretty.contains("<!>")) {
            shortOS = "error.os";
        } else if (sysInfo.osPretty.length() < 10) {
            shortOS = sysInfo.osPretty;
        } else {
            shortOS = sysInfo.osPretty.split(' ', Qt::SkipEmptyParts).first();
        }
        auto osTitle = new QLabel(shortOS);
        osTitle->setProperty("class", "titleText");
        leftLayout->addWidget(osTitle);

        auto versionLabel = new QLabel(sysInfo.osPretty);
        versionLabel->setProperty("class", "smallText");
        leftLayout->addWidget(versionLabel);

        auto infoBox = new QGroupBox;
        infoBox->setStyleSheet("QGroupBox { border: 1px solid #444; margin-top: 0; }");
        auto infoLayout = new QVBoxLayout(infoBox);

        auto headerLayout = new QHBoxLayout;
        headerLayout->addStretch(1);

        copyBtn = new QPushButton;
        copyBtn->setIcon(QIcon::fromTheme("edit-copy"));
        copyBtn->setFixedSize(24, 24);
        copyBtn->setStyleSheet("QPushButton { background: transparent; border: none; opacity: 0.7; }"
                               "QPushButton:hover { opacity: 1; background: rgba(255,255,255,0.1); border-radius: 4px; }");
        connect(copyBtn, &QPushButton::clicked, this, &SystemInfoPanel::copyAllInfo);

        refreshBtn = new QPushButton;
        refreshBtn->setIcon(QIcon::fromTheme("view-refresh"));
        refreshBtn->setFixedSize(24, 24);
        refreshBtn->setStyleSheet("QPushButton { background: transparent; border: none; opacity: 0.7; }"
                                  "QPushButton:hover { opacity: 1; background: rgba(255,255,255,0.1); border-radius: 4px; }");
        connect(refreshBtn, &QPushButton::clicked, this, &SystemInfoPanel::refreshInfo);

        headerLayout->addWidget(copyBtn);
        headerLayout->addWidget(refreshBtn);
        infoLayout->addLayout(headerLayout);

        infoData.append({"CPU Arch",    sysInfo.cpuArch});
        infoData.append({"CPU Model",   sysInfo.cpuModel});
        infoData.append({"CPU Cores",   sysInfo.cpuCores});
        infoData.append({"RAM",         sysInfo.ram});
        infoData.append({"Storage",     sysInfo.storage});
        infoData.append({"Hostname",    sysInfo.hostname});
        infoData.append({"Uptime",      sysInfo.uptime});
        infoData.append({"Kernel",      sysInfo.kernel});
        infoData.append({"User",        sysInfo.user});
        infoData.append({"Home",        sysInfo.homePath});
        infoData.append({"Documents",   sysInfo.documentsPath});
        infoData.append({"Downloads",   sysInfo.downloadsPath});
        infoData.append({"Time",        sysInfo.currentTime});
        infoData.append({"Install Date", sysInfo.installDate});

        const QString labelStyle = "color: white; font-size: 13px; margin: 4px 0; font-family: 'Nimbus Mono';";
        for (auto& item : infoData) {
            auto label = new QLabel(item.key + ": " + item.value);
            label->setStyleSheet(labelStyle);
            item.label = label;
            infoLayout->addWidget(label);
        }

        leftLayout->addWidget(infoBox);
        leftLayout->addStretch();

        auto rightLayout = new QVBoxLayout;
        rightLayout->setAlignment(Qt::AlignCenter);

        auto iconLabel = new GlowingLogo;
        auto pix = QPixmap(":/error.os.svgz").scaled(355, 440, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(pix);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("background: transparent;");
        connect(iconLabel, &GlowingLogo::triggerMiniGame, this, &SystemInfoPanel::launchMiniGame);
        rightLayout->addWidget(iconLabel);
        rightLayout->addStretch();

        mainLayout->addLayout(leftLayout, 1);
        mainLayout->addLayout(rightLayout, 1);
    }

private slots:
    void refreshInfo() {
        SystemInfoFetcher::Info sysInfo = SystemInfoFetcher::fetch();
        for (auto& item : infoData) {
            if (item.key == "CPU Arch") item.label->setText(item.key + ": " + sysInfo.cpuArch);
            else if (item.key == "CPU Model") item.label->setText(item.key + ": " + sysInfo.cpuModel);
            else if (item.key == "CPU Cores") item.label->setText(item.key + ": " + sysInfo.cpuCores);
            else if (item.key == "RAM") item.label->setText(item.key + ": " + sysInfo.ram);
            else if (item.key == "Storage") item.label->setText(item.key + ": " + sysInfo.storage);
            else if (item.key == "Hostname") item.label->setText(item.key + ": " + sysInfo.hostname);
            else if (item.key == "Uptime") item.label->setText(item.key + ": " + sysInfo.uptime);
            else if (item.key == "Kernel") item.label->setText(item.key + ": " + sysInfo.kernel);
            else if (item.key == "User") item.label->setText(item.key + ": " + sysInfo.user);
            else if (item.key == "Home") item.label->setText(item.key + ": " + sysInfo.homePath);
            else if (item.key == "Documents") item.label->setText(item.key + ": " + sysInfo.documentsPath);
            else if (item.key == "Downloads") item.label->setText(item.key + ": " + sysInfo.downloadsPath);
            else if (item.key == "Time") item.label->setText(item.key + ": " + sysInfo.currentTime);
            else if (item.key == "Install Date") item.label->setText(item.key + ": " + sysInfo.installDate);
        }
    }

    void copyAllInfo() {
        QString info;
        for (const auto& item : std::as_const(infoData)) {
            info += item.label->text() + "\n";
        }
        QGuiApplication::clipboard()->setText(info);

        copyBtn->setStyleSheet("QPushButton { background: rgba(30,144,255,0.3); border: none; opacity: 1; }");
        copyBtn->setToolTip("Copied!");

        QTimer::singleShot(800, [this]() {
            copyBtn->setStyleSheet("QPushButton { background: transparent; border: none; opacity: 0.7; }"
                                   "QPushButton:hover { opacity: 1; background: rgba(255,255,255,0.1); border-radius: 4px; }");
            copyBtn->setToolTip("");
        });
    }

    void launchMiniGame() {
        MiniGameDialog game(this);
        game.exec();
    }

private:
    struct InfoItem {
        QString key;
        QString value;
        QLabel* label = nullptr;
    };
    QList<InfoItem> infoData;
    QPushButton *copyBtn = nullptr;
    QPushButton *refreshBtn = nullptr;
};

class DriverManager : public QWidget {
    Q_OBJECT
public:
    DriverManager(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(20);

        QLabel *titleLabel = new QLabel("Driver Manager");
        titleLabel->setProperty("class", "titleText");
        layout->addWidget(titleLabel);

        statusLabel = new QLabel("Detecting hardware...");
        statusLabel->setProperty("class", "smallText");
        layout->addWidget(statusLabel);

        QGroupBox *nvidiaGroup = new QGroupBox;
        QHBoxLayout *nvidiaTitleLayout = new QHBoxLayout;
        QLabel *nvidiaIcon = new QLabel;
        nvidiaIcon->setPixmap(QIcon::fromTheme("video-display").pixmap(16, 16));
        QLabel *nvidiaTitle = new QLabel("NVIDIA Graphics");
        nvidiaTitle->setProperty("class", "smallText");
        nvidiaTitleLayout->addWidget(nvidiaIcon);
        nvidiaTitleLayout->addWidget(nvidiaTitle);
        nvidiaTitleLayout->addStretch();

        QVBoxLayout *nvidiaLayout = new QVBoxLayout(nvidiaGroup);
        nvidiaLayout->addLayout(nvidiaTitleLayout);

        installNvidiaBtn = new QPushButton(QIcon::fromTheme("download"), "Install NVIDIA Driver");
        installNvidiaBtn->setProperty("class", "plainButton");
        nvidiaLayout->addWidget(installNvidiaBtn);

        QLabel *secLabel = new QLabel("Checking error.os is recommended");
        secLabel->setAlignment(Qt::AlignCenter);
        secLabel->setProperty("class", "smallText");
        nvidiaLayout->addWidget(secLabel);
        nvidiaLayout->addStretch();

        layout->addWidget(nvidiaGroup);

        QGroupBox *printerGroup = new QGroupBox;
        QHBoxLayout *printerTitleLayout = new QHBoxLayout;
        QLabel *printerIcon = new QLabel;
        printerIcon->setPixmap(QIcon::fromTheme("printer").pixmap(16, 16));
        QLabel *printerTitle = new QLabel("Printer Support");
        printerTitle->setProperty("class", "smallText");
        printerTitleLayout->addWidget(printerIcon);
        printerTitleLayout->addWidget(printerTitle);
        printerTitleLayout->addStretch();

        QVBoxLayout *printerLayout = new QVBoxLayout(printerGroup);
        printerLayout->addLayout(printerTitleLayout);

        installPrinterBtn = new QPushButton(QIcon::fromTheme("download"), "Install Printer Drivers");
        installPrinterBtn->setProperty("class", "plainButton");
        printerLayout->addWidget(installPrinterBtn);

        layout->addWidget(printerGroup);

        removalGroup = new QGroupBox;
        QHBoxLayout *removalTitleLayout = new QHBoxLayout;
        QLabel *removalIcon = new QLabel;
        removalIcon->setPixmap(QIcon::fromTheme("edit-delete").pixmap(16, 16));
        QLabel *removalTitleLabel = new QLabel("Remove Unused Drivers (not recommended)");
        removalTitleLabel->setProperty("class", "smallText");
        removalTitleLayout->addWidget(removalIcon);
        removalTitleLayout->addWidget(removalTitleLabel);
        removalTitleLayout->addStretch();

        removalLayout = new QVBoxLayout(removalGroup);
        removalLayout->addLayout(removalTitleLayout);

        layout->addWidget(removalGroup);
        layout->addStretch();

        connect(installNvidiaBtn, &QPushButton::clicked, this, &DriverManager::installNvidiaDriver);
        connect(installPrinterBtn, &QPushButton::clicked, this, &DriverManager::installPrinterDrivers);

        detectHardware();
    }

private slots:
    void detectHardware() {
        QString gpuVendor, cpuVendor;

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

        QLayoutItem *child;
        while ((child = removalLayout->takeAt(1)) != nullptr) {
            delete child->widget();
            delete child;
        }

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
        runInTerminal("apt install -y nvidia-driver nvidia-settings", this,
                      "Installing NVIDIA driver...");
    }

    void installPrinterDrivers() {
        runInTerminal("sh -c 'apt update && apt upgrade -y && "
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
            runInTerminal(cmd, this, "Removing unused drivers...");
        }
    }

private:
    void addRemovalButton(const QString &label, const QStringList &pkgs) {
        QPushButton *btn = new QPushButton(QIcon::fromTheme("edit-delete"), label);
        btn->setProperty("class", "plainButton");
        removalLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, pkgs]() { confirmAndRemove(pkgs); });
    }

    QLabel *statusLabel;
    QPushButton *installNvidiaBtn;
    QPushButton *installPrinterBtn;
    QGroupBox *removalGroup;
    QVBoxLayout *removalLayout;
};
class AppInstaller : public QWidget {
    Q_OBJECT
public:
    AppInstaller(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);
        QLabel *titleLabel = new QLabel("App Installer");
        titleLabel->setProperty("class", "titleText");
        layout->addWidget(titleLabel);

        QLabel *helpLabel = new QLabel("Check boxes and install");
        helpLabel->setProperty("class", "smallText");
        layout->addWidget(helpLabel);

        tabs = new QTabWidget;
        tabs->addTab(createDpkgTab(), QIcon::fromTheme("package"), "dpkg");
        tabs->addTab(createFlatpakTab(), QIcon::fromTheme("application-x-flatpak"), "Flatpak");
        tabs->addTab(createWgetTab(), QIcon::fromTheme("download"), "wget");
        layout->addWidget(tabs);
    }

private:
    QTabWidget *tabs;

    struct DpkgApp {
        QString name;
        QString package;
        QString description;
        QString icon;
    };

    struct FlatpakApp {
        QString name;
        QString package;
        QString description;
        QString icon;
    };

    struct WgetApp {
        QString name;
        QString url;
        QString packageName;
        QString description;
        QString icon;
    };

    QWidget* createDpkgTab() {
        QWidget *widget = new QWidget;
        QVBoxLayout *vbox = new QVBoxLayout(widget);

        QLabel *intro = new QLabel("dpkg is the low-level tool that installs, removes, and manages .deb packages on Debian-based systems.");
        intro->setWordWrap(true);
        intro->setProperty("class", "smallText");
        vbox->addWidget(intro);

        QLabel *status = new QLabel("Select applications to install:");
        status->setProperty("class", "smallText");
        vbox->addWidget(status);

        QListWidget *list = new QListWidget;
        for (const auto &app : dpkgApps) {
            QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(app.icon), QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.package);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        QPushButton *btn = new QPushButton("Install Selected");
        btn->setProperty("class", "plainButton");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, list, status]() {
            QStringList pkgs;
            for (int i=0;i<list->count();++i) {
                auto item = list->item(i);
                if (item->checkState()==Qt::Checked)
                    pkgs << item->data(Qt::UserRole).toString();
            }
            if (pkgs.isEmpty()) { status->setText("No apps selected."); return; }
            status->setText("Installing via APT...");
            runInTerminal("apt install -y " + pkgs.join(' '), this, "Installing apps...");
        });

        return widget;
    }

    QWidget* createFlatpakTab() {
        QWidget *widget = new QWidget;
        QVBoxLayout *vbox = new QVBoxLayout(widget);

        QLabel *intro = new QLabel("Flatpak is a universal app system for Linux that lets you install sandboxed software.");
        intro->setWordWrap(true);
        intro->setProperty("class", "smallText");
        vbox->addWidget(intro);

        QLabel *status = new QLabel("Select applications to install:");
        status->setProperty("class", "smallText");
        vbox->addWidget(status);

        QListWidget *list = new QListWidget;
        for (const auto &app : flatpakApps) {
            QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(app.icon), QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.package);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        QPushButton *btn = new QPushButton("Install Selected");
        btn->setProperty("class", "plainButton");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, list, status]() {
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
                runInTerminal("apt update && apt install -y flatpak && "
                              "flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo",
                              this, "Installing Flatpak...");
            }

            QString cmd = "flatpak install -y " + pkgs.join(' ');
            status->setText("Installing selected Flatpak apps...");
            runInTerminal(cmd, this, "Installing Flatpak apps...");
        });

        return widget;
    }

    QWidget* createWgetTab() {
        QWidget *widget = new QWidget;
        QVBoxLayout *vbox = new QVBoxLayout(widget);

        QLabel *intro = new QLabel("wget installer fetches official .deb packages directly from vendor sites.");
        intro->setWordWrap(true);
        intro->setProperty("class", "smallText");
        vbox->addWidget(intro);

        QLabel *status = new QLabel("Select applications to install (deb packages only):");
        status->setProperty("class", "smallText");
        vbox->addWidget(status);

        QListWidget *list = new QListWidget;
        for (const auto &app : wgetApps) {
            QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(app.icon), QString("%1 - %2").arg(app.name, app.description));
            item->setData(Qt::UserRole, app.url);
            item->setData(Qt::UserRole + 1, app.packageName);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            list->addItem(item);
        }
        vbox->addWidget(list);

        QPushButton *btn = new QPushButton("Download and Install");
        btn->setProperty("class", "plainButton");
        vbox->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, list, status]() {
            QStringList selectedUrls;
            QStringList selectedPackages;
            for (int i=0;i<list->count();++i) {
                auto item = list->item(i);
                if (item->checkState()==Qt::Checked) {
                    selectedUrls << item->data(Qt::UserRole).toString();
                    selectedPackages << item->data(Qt::UserRole + 1).toString();
                }
            }
            if (selectedUrls.isEmpty()) { status->setText("No apps selected."); return; }

            status->setText("Downloading and installing via wget...");

            QStringList cmdParts;
            for (int i = 0; i < selectedUrls.size(); ++i) {
                QString url = selectedUrls[i];
                QString filename = url.section('/', -1);
                if (filename.contains("?")) {
                    filename = filename.split('?').first();
                }
                cmdParts << QString("wget -O /tmp/%1 '%2' && dpkg -i /tmp/%1 && rm -f /tmp/%1").arg(filename, url);
            }

            QString fullCmd = "sh -c '" + cmdParts.join(" && ") + " || apt-get -f install -y'";
            runInTerminal(fullCmd, this, "Installing selected apps...");
        });

        return widget;
    }

    QList<DpkgApp> dpkgApps = {
        {"Firefox", "firefox-esr", "Web Browser", "firefox"},
        {"VLC", "vlc", "Media Player", "vlc"},
        {"LibreOffice", "libreoffice", "Office Suite", "libreoffice"},
        {"GIMP", "gimp", "Image Editor", "gimp"},
        {"Geany", "geany", "Code Editor", "accessories-text-editor"},
        {"Thunderbird", "thunderbird", "Email Client", "thunderbird"},
        {"FileZilla", "filezilla", "FTP Client", "filezilla"},
        {"GCompris", "gcompris-qt", "Educational suite with 100+ activities", "gcompris"},
        {"KStars", "kstars", "Astronomy planetarium with star maps", "kstars"},
        {"Celestia", "celestia-gnome", "3D Universe simulator", "celestia"},
        {"Stellarium", "stellarium", "Realistic planetarium", "stellarium"},
        {"KAlgebra", "kalgebra", "Graphing calculator", "kalgebra"},
        {"KBruch", "kbruch", "Practice fractions", "kbruch"},
        {"Kig", "kig", "Interactive geometry", "kig"},
        {"Marble", "marble", "Virtual globe and world atlas", "marble"},
        {"TuxMath", "tuxmath", "Math game with Tux", "tuxmath"},
        {"TuxTyping", "tuxtype", "Typing tutor game", "tuxtype"},
        {"Scratch", "scratch", "Visual programming", "scratch"},
        {"KTurtle", "kturtle", "Educational programming", "kturtle"},
        {"SuperTux", "supertux", "2D Platformer", "supertux"},
        {"Extreme Tux Racer", "extremetuxracer", "Downhill racing with Tux", "extremetuxracer"},
        {"SuperTuxKart", "supertuxkart", "3D Kart Racing", "supertuxkart"},
        {"Warmux", "warmux", "Worms-like strategy game", "warmux"},
        {"FreedroidRPG", "freedroidrpg", "Sci-fi RPG with Tux", "freedroidrpg"},
        {"Pingus", "pingus", "Lemmings-style puzzle game", "pingus"},
        {"Inkscape", "inkscape", "Vector Graphics Editor", "inkscape"},
        {"Krita", "krita", "Digital Painting", "krita"},
        {"Pinta", "pinta", "Simple Image Editor", "pinta"},
        {"Okular", "okular", "PDF & Document Viewer", "okular"},
        {"Evince", "evince", "Lightweight PDF Viewer", "evince"},
        {"Calibre", "calibre", "E-book Manager", "calibre"},
        {"Simple Scan", "simple-scan", "Document Scanner", "simple-scan"},
        {"Remmina", "remmina", "Remote Desktop Client", "remmina"},
        {"Audacity", "audacity", "Audio Editor", "audacity"},
        {"Kdenlive", "kdenlive", "Video Editor", "kdenlive"},
        {"OBS Studio", "obs-studio", "Screen Recorder & Streaming", "obs-studio"},
        {"Shotwell", "shotwell", "Photo Manager", "shotwell"},
        {"Cheese", "cheese", "Webcam App", "cheese"},
        {"Guvcview", "guvcview", "Webcam Viewer/Recorder", "guvcview"},
        {"Rhythmbox", "rhythmbox", "Music Player", "rhythmbox"},
        {"Clementine", "clementine", "Music Player & Library Manager", "clementine"}
    };

    QList<FlatpakApp> flatpakApps = {
        {"Steam", "com.valvesoftware.Steam", "Gaming Platform", "steam"},
        {"Discord", "com.discordapp.Discord", "Chat & Voice", "discord"},
        {"Spotify", "com.spotify.Client", "Music Streaming", "spotify"},
        {"OBS Studio", "com.obsproject.Studio", "Screen Recorder", "obs-studio"},
        {"Kdenlive", "org.kde.kdenlive", "Video Editor", "kdenlive"},
        {"Audacity", "org.audacityteam.Audacity", "Audio Editor", "audacity"},
        {"Inkscape", "org.inkscape.Inkscape", "Vector Graphics", "inkscape"},
        {"Blender", "org.blender.Blender", "3D Creation Suite", "blender"},
        {"Chromium", "org.chromium.Chromium", "Web Browser", "chromium-browser"},
        {"Telegram", "org.telegram.desktop", "Messaging Client", "telegram"},
        {"OnlyOffice", "org.onlyoffice.desktopeditors", "Office Suite", "onlyoffice"},
        {"Remmina", "org.remmina.Remmina", "Remote Desktop", "remmina"},
        {"Krita", "org.kde.krita", "Digital Painting", "krita"},
        {"HandBrake", "fr.handbrake.ghb", "Video Transcoder", "handbrake"},
        {"Dolphin Emulator", "org.DolphinEmu.dolphin-emu", "GameCube/Wii Emulator", "dolphin-emu"},
        {"RetroArch", "org.libretro.RetroArch", "Multi-System Emulator", "retroarch"},
        {"PPSSPP", "org.ppsspp.PPSSPP", "PlayStation Portable Emulator", "ppsspp"},
        {"Prism Launcher", "org.prismlauncher.PrismLauncher", "Minecraft Launcher", "prismlauncher"},
        {"Lutris", "net.lutris.Lutris", "Open Gaming Platform", "lutris"},
        {"Heroic Games Launcher", "com.heroicgameslauncher.hgl", "Epic/GOG Games Launcher", "heroic"},
        {"Bottles", "com.usebottles.bottles", "Wine Manager", "bottles"},
        {"VLC", "org.videolan.VLC", "Media Player", "vlc"},
        {"melonDS", "net.kuribo64.melonDS", "Nintendo DS Emulator", "melonds"},
        {"ProtonUp-Qt", "net.davidotek.pupgui2", "Manage Proton-GE/Wine-GE", "protonup-qt"},
        {"Flatseal", "com.github.tchx84.Flatseal", "Manage Flatpak Permissions", "flatseal"},
        {"GIMP", "org.gimp.GIMP", "Image Editor", "gimp"},
        {"Firefox", "org.mozilla.firefox", "Web Browser", "firefox"},
        {"qBittorrent", "org.qbittorrent.qBittorrent", "Torrent Client", "qbittorrent"},
        {"0 A.D.", "com.play0ad.zeroad", "Real-Time Strategy Game", "0ad"},
        {"SuperTuxKart", "net.supertuxkart.SuperTuxKart", "Kart Racing Game", "supertuxkart"},
        {"Minetest", "net.minetest.Minetest", "Voxel Sandbox Game", "minetest"},
        {"Xonotic", "org.xonotic.Xonotic", "Fast-Paced FPS", "xonotic"},
        {"Warzone 2100", "net.wz2100.warzone2100", "Real-Time Strategy", "warzone2100"},
        {"FreeCiv", "org.freeciv.Freeciv", "Turn-Based Strategy", "freeciv"},
        {"OpenTTD", "org.openttd.OpenTTD", "Transport Tycoon Game", "openttd"},
        {"Visual Studio Code", "com.visualstudio.code", "Code Editor", "visual-studio-code"},
        {"LibreOffice", "org.libreoffice.LibreOffice", "Office Suite", "libreoffice"},
        {"Thunderbird", "org.mozilla.Thunderbird", "Email Client", "thunderbird"},
        {"Tux, of Math Command", "org.tux4kids.TuxMath", "Educational Math Game", "tuxmath"},
        {"Armagetron Advanced", "org.armagetronad.ArmagetronAdvanced", "Tron-Style Lightcycle Arena", "armagetronad"},
        {"The Battle for Wesnoth", "org.wesnoth.Wesnoth", "Turn-Based Strategy RPG", "wesnoth"},
        {"Supertux", "org.supertuxproject.SuperTux", "2D Platformer", "supertux"},
        {"Tremulous", "io.tremulous.Tremulous", "FPS/Strategy Hybrid", "tremulous"},
        {"Godot Engine", "org.godotengine.Godot", "Game Development Engine", "godot"},
        {"Tenacity", "org.tenacityaudio.Tenacity", "Audio Editor", "tenacity"},
        {"Zed", "app.zed.Zed", "High-Performance Code Editor", "zed"},
        {"Joplin", "net.cozic.joplin_desktop", "Note-Taking App", "joplin"},
        {"Signal", "org.signal.Signal", "Secure Messaging Client", "signal-desktop"},
        {"Element", "im.riot.Riot", "Matrix-Based Chat Client", "element"}
    };

    QList<WgetApp> wgetApps = {
        {"Google Chrome", "https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb", "google-chrome-stable", "Web Browser", "google-chrome"},
        {"Visual Studio Code", "https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64", "code", "Code Editor", "visual-studio-code"},
        {"Discord", "https://discord.com/api/download?platform=linux&format=deb", "discord", "Chat/Voice App", "discord"},
        {"Zoom", "https://zoom.us/client/latest/zoom_amd64.deb", "zoom", "Video Conferencing", "zoom"},
        {"Slack", "https://downloads.slack-edge.com/releases/linux/latest/slack-desktop-latest-amd64.deb", "slack-desktop", "Team Collaboration", "slack"},
        {"Brave Browser", "https://laptop-updates.brave.com/latest/dev-amd64.deb", "brave-browser", "Privacy Browser", "brave-browser"},
        {"Vivaldi", "https://downloads.vivaldi.com/stable/vivaldi-stable_latest_amd64.deb", "vivaldi-stable", "Customizable Browser", "vivaldi"},
        {"Opera", "https://deb.opera.com/opera-stable/latest_amd64.deb", "opera-stable", "Web Browser", "opera"},
        {"TeamViewer", "https://download.teamviewer.com/download/linux/teamviewer_amd64.deb", "teamviewer", "Remote Support", "teamviewer"},
        {"AnyDesk", "https://download.anydesk.com/linux/anydesk_latest_amd64.deb", "anydesk", "Remote Desktop", "anydesk"},
        {"Obsidian", "https://github.com/obsidianmd/obsidian-releases/releases/latest/download/obsidian-latest_amd64.deb", "obsidian", "Note-Taking App", "obsidian"}
    };
};

class AppRemover : public QWidget {
    Q_OBJECT
public:
    explicit AppRemover(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *titleLabel = new QLabel("Application Removal");
        titleLabel->setProperty("class", "titleText");
        layout->addWidget(titleLabel);

        QFrame *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(2);
        layout->addWidget(line);

        QLabel *disLabel = new QLabel(
            "Disclaimer: don't remove applications related to the core system.\n"
            "If you don't know about an application, search the web first."
            );
        disLabel->setWordWrap(true);
        disLabel->setProperty("class", "smallText");
        layout->addWidget(disLabel);

        QLabel *statusLabel = new QLabel("Enter application name to remove:");
        statusLabel->setProperty("class", "smallText");
        layout->addWidget(statusLabel);

        inputEdit = new QLineEdit();
        inputEdit->setPlaceholderText("Package name");

        QStringList installedPkgs = getInstalledPackages();
        QCompleter *completer = new QCompleter(installedPkgs, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        inputEdit->setCompleter(completer);
        layout->addWidget(inputEdit);

        removeBtn = new QPushButton(QIcon::fromTheme("edit-delete"), "Remove Application");
        removeBtn->setProperty("class", "plainButton");
        layout->addWidget(removeBtn);
        layout->addStretch();

        connect(removeBtn, &QPushButton::clicked, this, &AppRemover::removeAppByName);
    }

private:
    QLineEdit *inputEdit;
    QPushButton *removeBtn;

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
            return;
        }
        removeBtn->setEnabled(false);
        QString cmd = "apt remove -y " + pkg;
        runInTerminal(cmd, this, "Removing application...");
        removeBtn->setEnabled(true);
    }
};

class WineOptimizerDialog : public QDialog {
    Q_OBJECT
public:
    WineOptimizerDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Wine Configuration & Optimizer");
        setWindowIcon(QIcon::fromTheme("winecfg"));
        setMinimumSize(700, 650);
        resize(700, 650);

        auto mainLayout = new QVBoxLayout(this);

        auto titleLabel = new QLabel("Wine Optimizer & Configurator");

        titleLabel->setProperty("class", "titleText");
        mainLayout->addWidget(titleLabel);


        statusLabel = new QLabel("Checking Wine installation...");
        mainLayout->addWidget(statusLabel);

        auto tabs = new QTabWidget;
        tabs->addTab(makeScrollable(createInstallTab()), QIcon::fromTheme("system-software-install"), "Install/Update");
        tabs->addTab(makeScrollable(createOptimizeTab()), QIcon::fromTheme("preferences-system-performance"), "Optimize");
        tabs->addTab(makeScrollable(createConfigTab()), QIcon::fromTheme("preferences-system"), "Configure");
        tabs->addTab(makeScrollable(createCleanupTab()), QIcon::fromTheme("edit-clear-all"), "Cleanup");

        logArea = new QTextEdit;
        logArea->setReadOnly(true);
        logArea->setMaximumHeight(150);

        auto closeBtn = new QPushButton(QIcon::fromTheme("window-close"), "Close");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

        mainLayout->addWidget(tabs, 1);
        mainLayout->addWidget(logArea);
        mainLayout->addWidget(closeBtn);

        checkWineInstallation();
    }

private:
    QWidget* makeScrollable(QWidget *content) {
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setWidget(content);
        scroll->setFrameShape(QFrame::NoFrame);
        return scroll;
    }

    QWidget* createInstallTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel(
            "Wine allows you to run Windows applications on Linux.\n"
            "Install or update Wine to the latest version."
            );
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);

        wineVersionLabel = new QLabel("Wine Version: Checking...");
        layout->addWidget(wineVersionLabel);

        auto installStableBtn = new QPushButton(QIcon::fromTheme("system-software-install"), "Install Wine Stable");
        auto installStagingBtn = new QPushButton(QIcon::fromTheme("system-software-install"), "Install Wine Staging (Latest)");
        auto updateBtn = new QPushButton(QIcon::fromTheme("system-software-update"), "Update Wine");

        connect(installStableBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installWineStable);
        connect(installStagingBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installWineStaging);
        connect(updateBtn, &QPushButton::clicked, this, &WineOptimizerDialog::updateWine);

        layout->addWidget(installStableBtn);
        layout->addWidget(installStagingBtn);
        layout->addWidget(updateBtn);
        layout->addStretch();

        return widget;
    }

    QWidget* createOptimizeTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel(
            "Apply optimizations to improve Wine performance for gaming and applications."
            );
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);

        auto presetsGroup = new QGroupBox("Performance Presets");
        auto presetsLayout = new QVBoxLayout(presetsGroup);

        auto gamingBtn = new QPushButton(QIcon::fromTheme("applications-games"), "Gaming Preset (Max Performance)");
        auto balancedBtn = new QPushButton(QIcon::fromTheme("preferences-system"), "Balanced Preset");
        auto compatBtn = new QPushButton(QIcon::fromTheme("system-run"), "Compatibility Preset");

        connect(gamingBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyGamingPreset);
        connect(balancedBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyBalancedPreset);
        connect(compatBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyCompatPreset);

        presetsLayout->addWidget(gamingBtn);
        presetsLayout->addWidget(balancedBtn);
        presetsLayout->addWidget(compatBtn);

        auto optimGroup = new QGroupBox("Individual Optimizations");
        auto optimLayout = new QVBoxLayout(optimGroup);

        auto esyncBtn = new QPushButton(QIcon::fromTheme("media-playback-start"), "Enable ESYNC (Event Synchronization)");
        auto fsyncBtn = new QPushButton(QIcon::fromTheme("media-playback-start"), "Enable FSYNC (Fast Synchronization)");
        auto dxvkBtn = new QPushButton(QIcon::fromTheme("applications-graphics"), "Install DXVK (DirectX to Vulkan)");
        auto vkd3dBtn = new QPushButton(QIcon::fromTheme("applications-graphics"), "Install VKD3D (DirectX 12 to Vulkan)");
        auto largeAddrBtn = new QPushButton(QIcon::fromTheme("edit-find"), "Enable Large Address Aware");

        connect(esyncBtn, &QPushButton::clicked, this, &WineOptimizerDialog::enableEsync);
        connect(fsyncBtn, &QPushButton::clicked, this, &WineOptimizerDialog::enableFsync);
        connect(dxvkBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installDXVK);
        connect(vkd3dBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installVKD3D);
        connect(largeAddrBtn, &QPushButton::clicked, this, &WineOptimizerDialog::enableLargeAddr);

        optimLayout->addWidget(esyncBtn);
        optimLayout->addWidget(fsyncBtn);
        optimLayout->addWidget(dxvkBtn);
        optimLayout->addWidget(vkd3dBtn);
        optimLayout->addWidget(largeAddrBtn);

        layout->addWidget(presetsGroup);
        layout->addWidget(optimGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createConfigTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel("Configure Wine prefixes and Windows version emulation.");
        layout->addWidget(infoLabel);

        auto prefixGroup = new QGroupBox("Wine Prefix");
        auto prefixLayout = new QVBoxLayout(prefixGroup);

        auto prefixLabel = new QLabel("Select or create a Wine prefix:");
        prefixLayout->addWidget(prefixLabel);

        winePrefixCombo = new QComboBox;
        winePrefixCombo->addItem("Default (~/.wine)");
        winePrefixCombo->setEditable(false);
        prefixLayout->addWidget(winePrefixCombo);

        customPrefixEdit = new QLineEdit;
        customPrefixEdit->setPlaceholderText("Or enter custom prefix path...");
        prefixLayout->addWidget(customPrefixEdit);

        auto createPrefixBtn = new QPushButton(QIcon::fromTheme("list-add"), "Create New Prefix");
        connect(createPrefixBtn, &QPushButton::clicked, this, &WineOptimizerDialog::createPrefix);
        prefixLayout->addWidget(createPrefixBtn);

        auto winverGroup = new QGroupBox("Windows Version");
        auto winverLayout = new QVBoxLayout(winverGroup);

        auto winverCombo = new QComboBox;
        winverCombo->addItems({"Windows 10", "Windows 8.1", "Windows 7", "Windows XP"});
        winverLayout->addWidget(winverCombo);

        auto setWinverBtn = new QPushButton(QIcon::fromTheme("preferences-desktop"), "Set Windows Version");
        connect(setWinverBtn, &QPushButton::clicked, [this, winverCombo]() {
            setWindowsVersion(winverCombo->currentText());
        });
        winverLayout->addWidget(setWinverBtn);

        auto toolsGroup = new QGroupBox("Tools");
        auto toolsLayout = new QVBoxLayout(toolsGroup);

        auto winecfgBtn = new QPushButton(QIcon::fromTheme("preferences-system"), "Open Wine Configuration (winecfg)");
        auto regeditBtn = new QPushButton(QIcon::fromTheme("edit-find-replace"), "Open Wine Registry Editor");
        auto taskmgrBtn = new QPushButton(QIcon::fromTheme("utilities-system-monitor"), "Open Wine Task Manager");

        connect(winecfgBtn, &QPushButton::clicked, []() { QProcess::startDetached("winecfg"); });
        connect(regeditBtn, &QPushButton::clicked, []() { QProcess::startDetached("wine", {"regedit"}); });
        connect(taskmgrBtn, &QPushButton::clicked, []() { QProcess::startDetached("wine", {"taskmgr"}); });

        toolsLayout->addWidget(winecfgBtn);
        toolsLayout->addWidget(regeditBtn);
        toolsLayout->addWidget(taskmgrBtn);

        layout->addWidget(prefixGroup);
        layout->addWidget(winverGroup);
        layout->addWidget(toolsGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createCleanupTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel(
            "Clean up Wine prefixes, cache, and temporary files to free disk space."
            );
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);

        auto cacheBtn = new QPushButton(QIcon::fromTheme("edit-clear"), "Clear Wine Cache");
        auto tempBtn = new QPushButton(QIcon::fromTheme("edit-clear"), "Clean Temp Files");
        auto prefixBtn = new QPushButton(QIcon::fromTheme("user-trash"), "Remove Unused Prefixes");
        auto fullCleanBtn = new QPushButton(QIcon::fromTheme("edit-clear-all"), "Full Cleanup (All Above)");

        connect(cacheBtn, &QPushButton::clicked, this, &WineOptimizerDialog::clearCache);
        connect(tempBtn, &QPushButton::clicked, this, &WineOptimizerDialog::clearTemp);
        connect(prefixBtn, &QPushButton::clicked, this, &WineOptimizerDialog::cleanPrefixes);
        connect(fullCleanBtn, &QPushButton::clicked, this, &WineOptimizerDialog::fullCleanup);

        layout->addWidget(cacheBtn);
        layout->addWidget(tempBtn);
        layout->addWidget(prefixBtn);
        layout->addWidget(fullCleanBtn);
        layout->addStretch();

        return widget;
    }

private slots:
    void checkWineInstallation() {
        QString version = execProcess("wine", {"--version"});
        if (version.isEmpty()) {
            statusLabel->setText("Wine not installed");
            wineVersionLabel->setText("Wine Version: Not Installed");
            logArea->append("Wine is not installed on this system.");
        } else {
            statusLabel->setText(QString("Wine installed: %1").arg(version));
            wineVersionLabel->setText(QString("Wine Version: %1").arg(version));
            logArea->append(QString("Wine detected: %1").arg(version));
        }
    }

    void installWineStable() {
        logArea->append("\nInstalling Wine Stable...");
        QString cmd = "dpkg --add-architecture i386 && apt update && "
                      "apt install -y wine wine32 wine64 libwine libwine:i386 fonts-wine";
        runInTerminal(cmd, this, "Installing Wine Stable");
        QTimer::singleShot(3000, this, &WineOptimizerDialog::checkWineInstallation);
    }

    void installWineStaging() {
        logArea->append("\nInstalling Wine Staging...");
        QString cmd = "dpkg --add-architecture i386 && "
                      "wget -nc https://dl.winehq.org/wine-builds/winehq.key && "
                      "apt-key add winehq.key && "
                      "add-apt-repository 'deb https://dl.winehq.org/wine-builds/debian/ bookworm main' && "
                      "apt update && apt install -y --install-recommends winehq-staging";
        runInTerminal(cmd, this, "Installing Wine Staging");
        QTimer::singleShot(3000, this, &WineOptimizerDialog::checkWineInstallation);
    }

    void updateWine() {
        logArea->append("\nUpdating Wine...");
        runInTerminal("apt update && apt upgrade -y wine* winehq*", this, "Updating Wine");
        QTimer::singleShot(3000, this, &WineOptimizerDialog::checkWineInstallation);
    }

    void applyGamingPreset() {
        logArea->append("\nApplying Gaming Preset...");
        logArea->append("  • Enabling ESYNC");
        logArea->append("  • Enabling FSYNC");
        logArea->append("  • Installing DXVK");
        logArea->append("  • Optimizing registry settings");

        enableEsync();
        enableFsync();
        QProcess::execute("wine", {"reg", "add", "HKCU\\Software\\Wine\\Direct3D", "/v", "csmt", "/t", "REG_DWORD", "/d", "1", "/f"});
        QProcess::execute("wine", {"reg", "add", "HKCU\\Software\\Wine\\Direct3D", "/v", "MaxVersionGL", "/t", "REG_DWORD", "/d", "40600", "/f"});
        installDXVK();
        logArea->append("Gaming preset applied!");
    }

    void applyBalancedPreset() {
        logArea->append("\nApplying Balanced Preset...");
        enableEsync();
        logArea->append("Balanced preset applied!");
    }

    void applyCompatPreset() {
        logArea->append("\nApplying Compatibility Preset...");
        QProcess::execute("wine", {"reg", "add", "HKCU\\Software\\Wine\\Direct3D", "/v", "StrictDrawOrdering", "/t", "REG_DWORD", "/d", "1", "/f"});
        logArea->append("Compatibility preset applied!");
    }

    void enableEsync() {
        logArea->append("\nEnabling ESYNC...");
        QFile bashrc(QDir::homePath() + "/.bashrc");
        if (bashrc.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QTextStream in(&bashrc);
            QString content = in.readAll();
            if (!content.contains("WINEESYNC=1")) {
                QTextStream out(&bashrc);
                out << "\nexport WINEESYNC=1\n";
            }
            bashrc.close();
        }
        logArea->append("ESYNC enabled! Restart terminal to apply.");
    }

    void enableFsync() {
        logArea->append("\nEnabling FSYNC...");
        QFile bashrc(QDir::homePath() + "/.bashrc");
        if (bashrc.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QTextStream in(&bashrc);
            QString content = in.readAll();
            if (!content.contains("WINEFSYNC=1")) {
                QTextStream out(&bashrc);
                out << "\nexport WINEFSYNC=1\n";
            }
            bashrc.close();
        }
        logArea->append("FSYNC enabled! Restart terminal to apply.");
    }

    void installDXVK() {
        logArea->append("\nInstalling DXVK...");
        runInTerminal("apt install -y dxvk", this, "Installing DXVK");
    }

    void installVKD3D() {
        logArea->append("\nInstalling VKD3D...");
        runInTerminal("apt install -y vkd3d-compiler libvkd3d1 libvkd3d-dev", this, "Installing VKD3D");
    }

    void enableLargeAddr() {
        logArea->append("\nEnabling Large Address Aware...");
        QProcess::execute("wine", {"reg", "add", "HKLM\\System\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
                                   "/v", "LargeAddressAware", "/t", "REG_DWORD", "/d", "1", "/f"});
        logArea->append("Large Address Aware enabled!");
    }

    void createPrefix() {
        QString path = customPrefixEdit->text().trimmed();
        if (path.isEmpty()) {
            path = QFileDialog::getExistingDirectory(this, "Select Prefix Location", QDir::homePath());
        }
        if (!path.isEmpty()) {
            logArea->append(QString("\nCreating Wine prefix at: %1").arg(path));
            QProcess::execute("bash", {"-c", QString("WINEPREFIX='%1' wineboot").arg(path)});
            logArea->append("Prefix created!");
        }
    }

    void setWindowsVersion(const QString &version) {
        QString winver;
        if (version.contains("10")) winver = "win10";
        else if (version.contains("8")) winver = "win81";
        else if (version.contains("7")) winver = "win7";
        else winver = "winxp";

        logArea->append(QString("\nSetting Windows version to: %1").arg(version));
        QProcess::execute("wine", {"reg", "add", "HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion",
                                   "/v", "CurrentVersion", "/t", "REG_SZ", "/d", winver, "/f"});
        logArea->append("Windows version set!");
    }

    void clearCache() {
        logArea->append("\nClearing Wine cache...");
        QDir(QString("%1/.cache/wine").arg(QDir::homePath())).removeRecursively();
        QDir(QString("%1/.cache/winetricks").arg(QDir::homePath())).removeRecursively();
        logArea->append("Cache cleared!");
    }

    void clearTemp() {
        logArea->append("\nCleaning temp files...");
        QString tempPath = QDir::homePath() + "/.wine/drive_c/windows/temp";
        QDir tempDir(tempPath);
        if (tempDir.exists()) {
            QStringList entries = tempDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            for (int i = 0; i < entries.size(); ++i) {
                QString fullPath = tempDir.filePath(entries.at(i));
                if (QDir(fullPath).exists()) {
                    QDir(fullPath).removeRecursively();
                } else {
                    QFile::remove(fullPath);
                }
            }
        }
        logArea->append("Temp files cleaned!");
    }

    void cleanPrefixes() {
        logArea->append("\nScanning for unused prefixes...");
        logArea->append("Manual cleanup recommended. Check ~/.wine and custom locations.");
    }

    void fullCleanup() {
        if (QMessageBox::question(this, "Full Cleanup",
                                  "This will clear all Wine caches and temp files.\nContinue?") == QMessageBox::Yes) {
            clearCache();
            clearTemp();
            logArea->append("\nFull cleanup completed!");
        }
    }

private:
    QString execProcess(const QString &program, const QStringList &args, int timeout = 5000) {
        QProcess proc;
        proc.start(program, args);
        if (!proc.waitForFinished(timeout)) {
            proc.kill();
        }
        return QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
    }

    QLabel *statusLabel;
    QTextEdit *logArea;
    QLabel *wineVersionLabel;
    QComboBox *winePrefixCombo;
    QLineEdit *customPrefixEdit;
};
class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    SettingsPanel(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *iconLabel = new QLabel;
        QPixmap pix = QPixmap(":/txtlogo.svgz");
        if (pix.isNull()) {
            pix = QPixmap(256, 256);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Nimbus Mono", 50));
            painter.drawText(pix.rect(), Qt::AlignCenter, "err_");
            painter.end();
        } else {
            pix = pix.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        iconLabel->setPixmap(pix);
        iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(iconLabel);

        QLabel *infoLabel = new QLabel("version 3.0 (for  Neospace 2026)");
        infoLabel->setProperty("class", "smallText");
        layout->addWidget(infoLabel);

        QPushButton *wineBtn = new QPushButton("Wine Optimizer");
        wineBtn->setProperty("class", "plainButton");
        wineBtn->setIcon(QIcon::fromTheme("wine"));
        connect(wineBtn, &QPushButton::clicked, this, [this]() {
            WineOptimizerDialog dialog(this);
            dialog.exec();
        });
        layout->addWidget(wineBtn);

        QPushButton *settingsBtn = new QPushButton("System Settings");
        settingsBtn->setProperty("class", "plainButton");
        settingsBtn->setIcon(QIcon::fromTheme("preferences-system"));
        connect(settingsBtn, &QPushButton::clicked, this, []() {
            if (!QProcess::startDetached("systemsettings")) {
                QProcess::startDetached("lxqt-config");
            }
        });
        layout->addWidget(settingsBtn);

        QPushButton *githubBtn = new QPushButton("GitHub Repository");
        githubBtn->setProperty("class", "plainButton");
        githubBtn->setIcon(QIcon::fromTheme("git"));
        connect(githubBtn, &QPushButton::clicked, this, []() {
            QProcess::startDetached("xdg-open", {"https://github.com/zynomon/err_"});
        });
        layout->addWidget(githubBtn);

        QPushButton *wallpBtn = new QPushButton("Wallpaper Gallery");
        wallpBtn->setProperty("class", "plainButton");
        wallpBtn->setIcon(QIcon::fromTheme("preferences-wallpaper"));
        connect(wallpBtn, &QPushButton::clicked, this, []() {
            QProcess::startDetached("xdg-open", {"https://zynomon.github.io/errpaper"});
        });
        layout->addWidget(wallpBtn);

        QLabel *versionLabel = new QLabel("err_ v3.0 - error.dashboard for neospace");
        versionLabel->setAlignment(Qt::AlignCenter);
        versionLabel->setProperty("class", "smallText");
        layout->addWidget(versionLabel);

        QLabel *descLabel = new QLabel("this application is made for troubleshooting error.os's barebones issues");
        descLabel->setWordWrap(true);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setProperty("class", "smallText");
        layout->addWidget(descLabel);

        layout->addStretch();
    }
};
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("err_ - Your home");
        setWindowIcon(QIcon::fromTheme("err_"));
        setMinimumSize(800, 540);
        resize(900, 600);

        QWidget *mainWidget = new QWidget(this);
        setCentralWidget(mainWidget);

        QVBoxLayout *layout = new QVBoxLayout(mainWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QTabWidget *tabWidget = new QTabWidget(this);

        tabWidget->addTab(new SystemInfoPanel(), QIcon::fromTheme("system-help"), "System Info");
        tabWidget->addTab(new DriverManager(), QIcon::fromTheme("driver-manager"), "Drivers");
        tabWidget->addTab(new AppInstaller(), QIcon::fromTheme("system-installer"), "Install Apps");
        tabWidget->addTab(new AppRemover(), QIcon::fromTheme("edit-delete"), "Remove Apps");
        tabWidget->addTab(new SettingsPanel(), QIcon::fromTheme("preferences-other"), "Extra Settings");

        layout->addWidget(tabWidget);
    }
};

#include "err_.moc"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("err_");
    app.setApplicationVersion("3.0");
    app.setOrganizationName("error.os");

    QFont mono("Nimbus Mono");
    if (!mono.exactMatch()) mono = QFont("monospace");
    mono.setStyleHint(QFont::Monospace);
    app.setFont(mono);

    app.setStyleSheet(R"(
* {
    font-family: 'Nimbus Mono', 'Monospace';
    color: #dfe2ec;
}

QDialog {
    border: 1px solid #1a3cff;
    border-radius: 8px;
}

QMessageBox QPushButton {
    background-color: #112266;
    color: white;
    border: none;
    padding: 8px 16px;
    border-radius: 5px;
    font-weight: bold;
    font-size: 12px;
    min-width: 80px;
}
QMessageBox QPushButton:hover {
    background-color: #1a3cff;
}

QLabel[class="titleText"], QPushButton[class="titleText"] {
    font-size: 32px;
    font-weight: bold;
    color: #dfe2ec;
    border: none;
}

QLabel[class="smallText"], QPushButton[class="smallText"] {
    font-size: 13px;
    color: #cccccc;
    border: none;
}

QPushButton[class="plainButton"] {
    background-color: #112266;
    color: white;
    border: none;
    padding: 10px 16px;
    border-radius: 5px;
    font-weight: bold;
    font-size: 12px;
}
QPushButton[class="plainButton"]:hover {
    background-color: #1a3cff;
}

QLineEdit, QTextEdit, QComboBox, QListWidget {
    background-color: #111;
    color: white;
    border: 1px solid #223355;
    padding: 5px;
    border-radius: 4px;
}

QGroupBox {
    color: #ffffff;
    font-weight: bold;
    font-size: 13px;
    margin-top: 12px;
    padding-top: 12px;
    border: 1px solid #1a3cff;
    border-radius: 6px;
}
QGroupBox::title {
    left: 12px;
    padding: 0 8px;
    color: #00bfff;
}



QScrollArea {
    border: none;
}
QScrollBar:vertical {
    width: 12px;
    border-radius: 6px;
}
QScrollBar::handle:vertical {
    background-color: #1a3cff;
    border-radius: 6px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background-color: #00bfff;
}
QTabWidget::pane {
    border: 1px solid #2a3245;
    background-color: #0d0d0d;
    border-radius: 6px;
    padding: 4px;
}
QTabBar::tab {
    background-color: #1a1a1a;
    color: #9ca0b0;
    padding: 8px 20px;
    font-family: 'Nimbus Mono';
    font-size: 11pt;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    margin: 2px;
}
QTabBar::tab:selected {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a86ff, stop:1 #001f54);
    color: #ffffff;
    font-weight: bold;
    border: 1px solid #3a86ff;
}
QTabBar::tab:!selected:hover {
    background-color: #2a3245;
    color: #ffffff;
    border: 1px solid #5a6fff;
    font-weight: bold;
}QTabWidget::pane {
    border: 1px solid #2a3245;
    background-color: #0d0d0d;
    border-radius: 6px;
    padding: 4px;
}
QTabBar::tab {
    background-color: #1a1a1a;
    color: #9ca0b0;
    padding: 8px 20px;
    font-family: 'Nimbus Mono';
    font-size: 11pt;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    margin: 2px;
}
QTabBar::tab:selected {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a86ff, stop:1 #001f54);
    color: #ffffff;
    font-weight: bold;
    border: 1px solid #3a86ff;
}
QTabBar::tab:!selected:hover {
    background-color: #2a3245;
    color: #ffffff;
    border: 1px solid #5a6fff;
    font-weight: bold;
}
)");

    MainWindow window;
    window.show();

    return app.exec();
}
