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
            "QDialog { background: #000; font-family: 'Nimbus Mono'; color: #fff; }"
            "QLabel { font-family:  'Nimbus Mono'; color: #fff; }"
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
    return QString("");
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
        args << "-e" << sudoCmd;

    QProcess::startDetached(terminal, args);
    dlg->showInfo(desc + "\n Yes, a terminal will open for authentication.");
    dlg->show();
    QTimer::singleShot(2000, dlg, &QDialog::accept);
}


// --- Utility: System Info (Pure Qt - NO QProcess, NO Terminals) ---
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

    QProcess p; p.start("lscpu"); p.waitForFinished(600);
    for (QString line : p.readAllStandardOutput().split('\n')) {
        if (line.toLower().startsWith("model name"))
            return line.section(':',1).trimmed();
    }
    return "Unknown";
}

QString getCPUCoreCount() {
    // 1. QSysInfo → 2. /proc → 3. nproc
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

    QProcess p; p.start("nproc"); p.waitForFinished(300);
    QString out = p.readAllStandardOutput().trimmed();
    return !out.isEmpty() ? out : "Unknown";
}

QString getPhysicalCoreCount() {
    // 1. QSysInfo → 2. /proc → 3. lscpu
    if (int cores = QThread::idealThreadCount(); cores > 0)
        return QString::number(cores);

    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        int pkgs = 0, lastPkg = -1;
        while (!f.atEnd()) {
            QString line = f.readLine();
            if (line.toLower().startsWith("physical id")) {
                int pkg = line.section(':',1).trimmed().toInt();
                if (pkg != lastPkg) { pkgs++; lastPkg = pkg; }
            }
        }
        if (pkgs > 0) return QString::number(pkgs);
    }

    QProcess p; p.start("lscpu"); p.waitForFinished(500);
    QString out = p.readAllStandardOutput();

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
    return "Unknown";
}

QString getHostname() {
    return QSysInfo::machineHostName();
}

QString getUptime() {
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

QString getRam() {
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

QString getInstallDate() {
    QFileInfo root("/");
    if (root.lastModified().isValid()) return root.lastModified().toString("dd MMM yyyy");

    QProcess p; p.start("stat", {"/"}); p.waitForFinished();

    return "Not available";
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
        jumpCountLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-family: 'Nimbus Mono';");
        jumpCountLabel->setGeometry(10, 8, 120, 20);

        powerupStatusLabel = new QLabel("Powerups: 0", this);
        powerupStatusLabel->setStyleSheet("color: #ff8888; font-family: 'Nimbus Mono';");
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

        font.setFamily(QString::fromUtf8("OpenMoji"));
        if (!QFontInfo(font).family().contains("Noto") && QFontInfo(font).pointSize() == 0) {
            font.setFamily(QString());
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

        if (obstacleSpeed > maxObstacleSpeed) obstacleSpeed = maxObstacleSpeed;
        powerupStatusLabel->setText(QString("Powerups: %1").arg(permanentPowerups));
        powerupStatusLabel->setStyleSheet("color: #88ff88; font-weight: bold; font-family: 'Nimbus Mono';");
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

// --- System Info Panel ---
class SystemInfoPanel : public QWidget {
    Q_OBJECT
public:
    SystemInfoPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto mainLayout = new QHBoxLayout(this);


        auto leftLayout = new QVBoxLayout;

        QString fullOS = getOSInfo();

        QString shortOS;
        if (fullOS.contains("<!>")) {
            shortOS = "error.os";
        } else if (fullOS.length() < 10) {
            shortOS = fullOS;
        } else {
            shortOS = fullOS.split(' ', Qt::SkipEmptyParts).first();
        }
        auto osTitle = new QLabel(shortOS);
        osTitle->setStyleSheet(
            "font-family: 'Nimbus Mono PS', 'Nimbus Mono';"
            "font-size: 28px;"
            "font-weight: bold;"
            "color: #dfe2ec;"
            "margin-bottom: 10px;"
            );
        leftLayout->addWidget(osTitle);

        auto versionLabel = new QLabel(fullOS);
        versionLabel->setStyleSheet(
            "font-family: 'Nimbus Mono PS', 'Nimbus Mono';"
            "font-size: 14px;"
            "color: gray;"
            "margin-bottom: 15px;"
            );
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


        infoData.append({"CPU Arch",    [&](){ return getCPUArch(); }});
        infoData.append({"CPU Model",   [&](){ return getCPUModel(); }});
        infoData.append({"CPU Cores",   [&](){ return getCPUCoreCount(); }});
        infoData.append({"RAM",         [&](){ return getRam(); }});
        infoData.append({"Storage",     [&](){ return getStorage(); }});
        infoData.append({"Hostname",    [&](){ return getHostname(); }});
        infoData.append({"Uptime",      [&](){ return getUptime(); }});
        infoData.append({"Kernel",      [&](){ return getKernel(); }});
        infoData.append({"User",        [&](){ return getUser(); }});
        infoData.append({"Home",        [&](){ return getHomePath(); }});
        infoData.append({"Documents",   [&](){ return getDocumentsPath(); }});
        infoData.append({"Downloads",   [&](){ return getDownloadsPath(); }});
        infoData.append({"Time",        [&](){ return QDateTime::currentDateTime().toString(); }});
        infoData.append({"Install Date", [&](){ return "error.os install date: " + getInstallDate(); }});


        const QString labelStyle = "color: white; font-size: 13px; margin: 4px 0; font-family: 'Nimbus Mono';";
        for (auto& item : infoData) {
            auto label = new QLabel(item.key + ": " + item.value());
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
        for (auto& item : infoData) {
            item.label->setText(item.key + ": " + item.value());
        }
    }

    void copyAllInfo() {
        QString info;
        for (const auto& item : infoData) {
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
        std::function<QString()> value;
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
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(20);

        auto titleLabel = new QLabel("Driver Manager");
        titleLabel->setStyleSheet(
            "font-size: 32px; font-weight: bold; color: #dfe2ec; margin-bottom: 10px;"
            "font-family: 'Nimbus Mono';"
            );
        statusLabel = new QLabel("Detecting hardware...");
        statusLabel->setStyleSheet(
            "color: #cccccc; margin-bottom: 20px; font-size: 20px;"
            "font-family: 'Nimbus Mono';"
            );

        auto nvidiaGroup = new QGroupBox("🔰 NVIDIA Graphics");
        nvidiaGroup->setStyleSheet(groupBoxStyle());

        auto nvidiaLayout = new QVBoxLayout(nvidiaGroup);

        installNvidiaBtn = styledButton("Install NVIDIA Driver");
        nvidiaLayout->addWidget(installNvidiaBtn);

        auto secLabel = new QLabel("Checking error.doc is recommended");
        secLabel->setStyleSheet("font-size: 8px; font-weight: bold; color: #dfe2ec;");
        secLabel->setAlignment(Qt::AlignCenter);
        nvidiaLayout->addWidget(secLabel);

        nvidiaLayout->addStretch();
        nvidiaLayout->setSpacing(6);
        nvidiaLayout->setContentsMargins(10, 10, 10, 8);

        layout->addWidget(nvidiaGroup);

        auto printerGroup = new QGroupBox("🖨️  Printer Support");
        printerGroup->setStyleSheet(groupBoxStyle());
        auto printerLayout = new QVBoxLayout(printerGroup);
        installPrinterBtn = styledButton("Install Printer Drivers");
        printerLayout->addWidget(installPrinterBtn);

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
               "font-family: 'Nimbus Mono'; }"
               "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 10px; color: #00bfff; }";
    }

    QPushButton* styledButton(const QString &text) {
        auto *btn = new QPushButton(text);
        btn->setStyleSheet("QPushButton { background: #212679; color: #ffffff; border: 1px solid #5a6fff;;"
                           "padding: 12px 20px; border-radius: 6px; font-family: 'Nimbus Mono';"
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
        auto titleLabel = new QPushButton(QIcon::fromTheme("download"), "App installer");
        titleLabel->setFlat(true);
        titleLabel->setStyleSheet(
            "QPushButton { "
            "   background: transparent; "
            "   color: #dfe2ec; "
            "   font-family: 'Nimbus Mono'; "
            "   font-size: 24px; "
            "   font-weight: bold; "
            "   padding: 12px 24px; "
            "   border: none; "
            "   border-radius: 0; "
            "   margin: 0; "
            "   text-align: left; "
            "   cursor: arrow; "
            "} "
            "QPushButton:disabled { "
            "   color: #aaaaaa; "
            "   background: transparent; "
            "   cursor: arrow; "
            "} "
            );
        titleLabel->setEnabled(false);
        titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        layout->addWidget(titleLabel);
        auto helpLabel = new QLabel("Check  boxes and install");
        helpLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00BFFF; margin: 2px 0;");
        layout->addWidget(helpLabel);

        tabs = new QTabWidget;
        tabs->addTab(createDpkgTab(), QIcon::fromTheme("tux"), "dpkg");
        tabs->addTab(createFlatpakTab(), QIcon::fromTheme("application-vnd.flatpak"), "Flatpak");
        tabs->addTab(createWgetTab(), QIcon::fromTheme("abrowser"), "wget");
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
        status->setStyleSheet("color: white; font-family: 'Nimbus Mono';");
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
        status->setStyleSheet("color: white; font-family: 'Nimbus Mono';");
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
        status->setStyleSheet("color: white; font-family: 'Nimbus Mono';");
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

    // --- Flatpak Apps (from Flathub) ---
    QList<App> flatpakApps = {
        {"Steam", "com.valvesoftware.Steam", "Gaming Platform"},
        {"Discord", "com.discordapp.Discord", "Chat & Voice"},
        {"Spotify", "com.spotify.Client", "Music Streaming"},
        {"OBS Studio", "com.obsproject.Studio", "Screen Recorder"},
        {"Kdenlive", "org.kde.kdenlive", "Video Editor"},
        {"Audacity", "org.audacityteam.Audacity", "Audio Editor"},
        {"Inkscape", "org.inkscape.Inkscape", "Vector Graphics"},
        {"Blender", "org.blender.Blender", "3D Creation Suite"},
        {"Chromium", "org.chromium.Chromium", "Web Browser"},
        {"Telegram", "org.telegram.desktop", "Messaging Client"},
        {"OnlyOffice", "org.onlyoffice.desktopeditors", "Office Suite"},
        {"Remmina", "org.remmina.Remmina", "Remote Desktop"},
        {"Krita", "org.kde.krita", "Digital Painting"},
        {"HandBrake", "fr.handbrake.ghb", "Video Transcoder"},
        {"Dolphin Emulator", "org.DolphinEmu.dolphin-emu", "GameCube/Wii Emulator"},
        {"RetroArch", "org.libretro.RetroArch", "Multi-System Emulator Frontend"},
        {"PPSSPP", "org.ppsspp.PPSSPP", "PlayStation Portable Emulator"},
        {"Prism Launcher", "org.prismlauncher.PrismLauncher", "Minecraft Launcher"},
        {"Lutris", "net.lutris.Lutris", "Open Gaming Platform"},
        {"Heroic Games Launcher", "com.heroicgameslauncher.hgl", "Epic/GOG Games Launcher"},
        {"Bottles", "com.usebottles.bottles", "Wine Manager for Games/Apps"},
        {"VLC", "org.videolan.VLC", "Media Player"},
        {"melonDS", "net.kuribo64.melonDS", "Nintendo DS Emulator"},
        {"ProtonUp-Qt", "net.davidotek.pupgui2", "Manage Proton-GE/Wine-GE"},
        {"Flatseal", "com.github.tchx84.Flatseal", "Manage Flatpak Permissions"},
        {"GIMP", "org.gimp.GIMP", "Image Editor"},
        {"Firefox", "org.mozilla.firefox", "Web Browser"},
        {"qBittorrent", "org.qbittorrent.qBittorrent", "Torrent Client"},
        {"0 A.D.", "com.play0ad.zeroad", "Real-Time Strategy Game"},
        {"SuperTuxKart", "net.supertuxkart.SuperTuxKart", "Kart Racing Game"},
        {"Minetest", "net.minetest.Minetest", "Voxel Sandbox Game"},
        {"Xonotic", "org.xonotic.Xonotic", "Fast-Paced FPS"},
        {"Warzone 2100", "net.wz2100.warzone2100", "Real-Time Strategy"},
        {"FreeCiv", "org.freeciv.Freeciv", "Turn-Based Strategy"},
        {"OpenTTD", "org.openttd.OpenTTD", "Transport Tycoon Game"},
        {"Visual Studio Code", "com.visualstudio.code", "Code Editor"},
        {"LibreOffice", "org.libreoffice.LibreOffice", "Office Suite"},
        {"Thunderbird", "org.mozilla.Thunderbird", "Email Client"},
        {"Cave Story NX", "com.gitlab.coringao.cavestory-nx", "Metroidvania Platformer (NXEngine-evo)"}, //[](https://flathub.org/apps/com.gitlab.coringao.cavestory-nx)
        {"Shovel Knight", "com.yachtclubgames.ShovelKnight", "Retro Platformer Adventure"},
        {"Hollow Knight", "com.teamcherry.HollowKnight", "Metroidvania Action Game"},
        {"Celeste", "com.mattmakesgames.Celeste", "Precision Platformer"},
        {"Dead Cells", "com.motiontwin.DeadCells", "Roguevania Action Platformer"},
        {"Stardew Valley", "com.chucklefish.StardewValley", "Farming Sim RPG"},
        {"Endless Sky", "org.endlesssky.endless_sky", "Space Trading/Combat Sim"}, //[](https://www.makeuseof.com/best-free-linux-flatpak-games/)
        {"Tux, of Math Command", "org.tux4kids.TuxMath", "Educational Math Game"},
        {"Armagetron Advanced", "org.armagetronad.ArmagetronAdvanced", "Tron-Style Lightcycle Arena"},
        {"The Battle for Wesnoth", "org.wesnoth.Wesnoth", "Turn-Based Strategy RPG"},
        {"Supertux", "org.supertuxproject.SuperTux", "2D Platformer (Mario-like)"},
        {"Tremulous", "io.tremulous.Tremulous", "FPS/Strategy Hybrid"},
        {"OpenSpades", "net.yvt.OpenSpades", "Voxel-Based FPS"},
        {"Godot Engine", "org.godotengine.Godot", "Game Development Engine"},
        {"Tenacity", "org.tenacityaudio.Tenacity", "Audio Editor (Audacity Fork)"},
        {"Zed", "app.zed.Zed", "High-Performance Code Editor"},
        {"Joplin", "net.cozic.joplin_desktop", "Note-Taking and To-Do App"},
        {"Signal", "org.signal.Signal", "Secure Messaging Client"},
        {"Element", "im.riot.Element", "Matrix-Based Chat Client"}
    };
    // --- wget Apps (direct .deb installers) ---
    QList<App> wgetApps = {
        {
            "WPS Office",
            "wget -O /tmp/wps-office.deb https://wdl1.pcfg.cache.wpscdn.com/wpsdl/wpsoffice/download/linux/latest/wps-office_amd64.deb && sudo dpkg -i /tmp/wps-office.deb || sudo apt -f install -y",
            "Office Suite (from WPS CDN - direct latest .deb)"
        },
        {
            "Visual Studio Code",
            "wget -O /tmp/code.deb https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64 && sudo apt install /tmp/code.deb",
            "Code Editor (from Microsoft - direct always-latest)"
        },
        {
            "Apache OpenOffice",
            "wget -O /tmp/openoffice.tar.gz https://downloads.apache.org/openoffice/4.1.16/binaries/en-US/Apache_OpenOffice_4.1.16_Linux_x86-64_install-deb_en-US.tar.gz && tar -xzf /tmp/openoffice.tar.gz -C /tmp && sudo dpkg -i /tmp/Apache_OpenOffice_4.1.16_Linux_x86-64_install-deb_en-US/DEBS/*.deb && sudo dpkg -i /tmp/Apache_OpenOffice_4.1.16_Linux_x86-64_install-deb_en-US/desktop-integration/*.deb || sudo apt -f install -y",
            "Office Suite (from Apache - latest 4.1.16 direct tar.gz with DEBS)"
        },
        {
            "Google Chrome",
            "wget -O /tmp/google-chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb && sudo dpkg -i /tmp/google-chrome.deb || sudo apt -f install -y",
            "Web Browser (from Google - direct always-latest)"
        },
        {
            "TeamViewer",
            "wget -O /tmp/teamviewer.deb https://download.teamviewer.com/download/linux/teamviewer_amd64.deb && sudo dpkg -i /tmp/teamviewer.deb || sudo apt -f install -y",
            "Remote Support (from TeamViewer - direct always-latest)"
        },
        {
            "Opera One",
            "wget -O /tmp/opera.deb https://deb.opera.com/opera-stable/latest_amd64.deb && sudo dpkg -i /tmp/opera.deb || sudo apt -f install -y",
            "Web Browser (from Opera repo - direct latest redirect)"
        },
        {
            "Lutris",
            "wget -O /tmp/lutris.deb https://github.com/lutris/lutris/releases/latest/download/lutris_amd64.deb && sudo dpkg -i /tmp/lutris.deb || sudo apt -f install -y",
            "Game Launcher (from Lutris GitHub - latest redirect, instant file)"
        },
        {
            "Itch.io App",
            "wget -O /tmp/itch-setup https://itch.io/app-download/linux && chmod +x /tmp/itch-setup && /tmp/itch-setup",
            "Indie Game Launcher (from Itch.io - direct setup executable)"
        },
        {
            "Canva Desktop",
            "wget -O /tmp/canva.deb https://github.com/vikdevelop/canvadesktop/releases/latest/download/canva-desktop_amd64.deb && sudo dpkg -i /tmp/canva.deb || sudo apt -f install -y",
            "Design Tool (community Electron wrapper - GitHub latest redirect)"
        },
        {
            "Zoom",
            "wget -O /tmp/zoom.deb https://zoom.us/client/latest/zoom_amd64.deb && sudo dpkg -i /tmp/zoom.deb || sudo apt -f install -y",
            "Video Conferencing (from Zoom - direct always-latest)"
        },
        {
            "ProtonVPN",
            "wget -O /tmp/protonvpn.deb https://repo.protonvpn.com/debian/dists/stable/main/binary-all/protonvpn-stable-release_1.0.8_all.deb && sudo dpkg -i /tmp/protonvpn.deb && sudo apt update && sudo apt install protonvpn",
            "VPN Client (from ProtonVPN - direct repo package, then install app)"
        },
        {
            "Discord",
            "wget -O /tmp/discord.deb \"https://discord.com/api/download?platform=linux&format=deb\" && sudo dpkg -i /tmp/discord.deb || sudo apt -f install -y",
            "Chat/Voice App (from Discord - direct always-latest .deb)"
        },
        {
            "Slack",
            "wget -O /tmp/slack.deb https://downloads.slack-edge.com/releases/linux/latest/slack-desktop-latest-amd64.deb && sudo dpkg -i /tmp/slack.deb || sudo apt -f install -y",
            "Team Collaboration (from Slack - direct always-latest .deb)"
        },
        {
            "Brave Browser",
            "wget -O /tmp/brave.deb https://laptop-updates.brave.com/latest/dev-amd64.deb && sudo dpkg -i /tmp/brave.deb || sudo apt -f install -y",
            "Privacy Browser (from Brave - direct always-latest .deb)"
        },
        {
            "Microsoft Edge",
            "wget -O /tmp/ms-edge.deb https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-stable/microsoft-edge-stable_latest_amd64.deb && sudo dpkg -i /tmp/ms-edge.deb || sudo apt -f install -y",
            "Web Browser (from Microsoft - direct latest stable .deb redirect)"
        },
        {
            "Vivaldi",
            "wget -O /tmp/vivaldi.deb https://downloads.vivaldi.com/stable/vivaldi-stable_latest_amd64.deb && sudo dpkg -i /tmp/vivaldi.deb || sudo apt -f install -y",
            "Customizable Browser (from Vivaldi - direct latest stable .deb)"
        },
        {
            "Obsidian",
            "wget -O /tmp/obsidian.deb https://github.com/obsidianmd/obsidian-releases/releases/latest/download/obsidian-latest_amd64.deb && sudo dpkg -i /tmp/obsidian.deb || sudo apt -f install -y",
            "Note-Taking App (from Obsidian GitHub - latest redirect .deb)"
        },
        {
            "Bitwarden",
            "wget -O /tmp/bitwarden.AppImage https://github.com/bitwarden/clients/releases/latest/download/Bitwarden-latest-x86_64.AppImage && chmod +x /tmp/bitwarden.AppImage && /tmp/bitwarden.AppImage",
            "Password Manager (from Bitwarden GitHub - direct latest AppImage, run portable)"
        },
        {
            "AnyDesk",
            "wget -O /tmp/anydesk.deb https://download.anydesk.com/linux/anydesk_latest_amd64.deb && sudo dpkg -i /tmp/anydesk.deb || sudo apt -f install -y",
            "Remote Desktop (from AnyDesk - direct latest .deb)"
        }
    };
};

// APP remover

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
            "font-family: 'Nimbus Mono'; font-size: small;"
            );

        statusLabel = new QLabel("Enter application name to remove:");
        statusLabel->setStyleSheet(
            "color: white; margin-bottom: 10px; "
            "font-family: 'Nimbus Mono';"
            );

        inputEdit = new QLineEdit();
        inputEdit->setPlaceholderText("Some very new packages may not be suggested");
        inputEdit->setStyleSheet(
            "background-color: #111; color: white; "
            "border: 1px solid #223355; padding: 8px; border-radius: 4px; "
            "font-family: 'Nimbus Mono';"
            );

        QStringList installedPkgs = getInstalledPackages();
        QCompleter *completer = new QCompleter(installedPkgs, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        inputEdit->setCompleter(completer);

        removeBtn = new QPushButton("Remove Application");
        removeBtn->setStyleSheet(
            "QPushButton { background-color: #112266; color: white; border: none; "
            "padding: 12px; border-radius: 5px; font-weight: bold; "
            "font-family: 'Nimbus Mono'; }"
            "QPushButton:hover { background-color: #1a3cff; "

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

// settings winer
class WineOptimizerDialog : public QDialog {
    Q_OBJECT
public:
    WineOptimizerDialog(QWidget *parent = nullptr) : QDialog(parent) {

        setWindowTitle("Wine Configuration & Optimizer");
        setWindowIcon(QIcon::fromTheme("winecfg"));
        setMinimumSize(100 , 200);
        resize(700, 650);
        setStyleSheet("QDialog { background: #0a0a0a; color: #fff; }");

        auto mainLayout = new QVBoxLayout(this);

        auto titleLabel = new QLabel("Wine Optimizer & Configurator");
        titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00bfff; margin-bottom: 10px;");

        statusLabel = new QLabel("Checking Wine installation...");
        statusLabel->setStyleSheet("color: #0f0; font-family: 'Nimbus Mono'; font-size: 11px;");

        auto tabs = new QTabWidget;
        tabs->setStyleSheet(
            "QTabWidget::pane { border: 1px solid #333; background: #0d0d0d; }"
            "QTabBar::tab { background: #1a1a1a; color: #ccc; padding: 8px 16px; }"
            "QTabBar::tab:selected { background: #1a3cff; color: white; font-weight: bold; }"
            );

        tabs->addTab(createInstallTab(), QIcon::fromTheme("system-software-install"), "Install/Update");
        tabs->addTab(createOptimizeTab(), QIcon::fromTheme("preferences-system-performance"), "Optimize");
        tabs->addTab(createConfigTab(), QIcon::fromTheme("preferences-system"), "Configure");
        tabs->addTab(createCleanupTab(), QIcon::fromTheme("edit-clear-all"), "Cleanup");

        logArea = new QTextEdit;
        logArea->setReadOnly(true);
        logArea->setMaximumHeight(150);
        logArea->setStyleSheet(
            "QTextEdit { background: #000; color: #0f0; border: 1px solid #333; "
            "font-family: 'Nimbus Mono',; font-size: 10px; padding: 5px; }"
            );

        auto closeBtn = new QPushButton(QIcon::fromTheme("window-close"), "Close");
        closeBtn->setStyleSheet(
            "QPushButton { background: #444; color: white; padding: 10px 20px; "
            "border-radius: 4px; font-weight: bold; } QPushButton:hover { background: #666; }"
            );
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(statusLabel);
        mainLayout->addWidget(tabs, 1);
        mainLayout->addWidget(logArea);
        mainLayout->addWidget(closeBtn);

        checkWineInstallation();
    }

private:
    QLabel *statusLabel;
    QTextEdit *logArea;
    QLabel *wineVersionLabel;
    QComboBox *winePrefixCombo;
    QLineEdit *customPrefixEdit;

    QWidget* createInstallTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel(
            "Wine allows you to run Windows applications on Linux.\n"
            "Install or update Wine to the latest version."
            );
        infoLabel->setWordWrap(true);
        infoLabel->setStyleSheet("color: #aaa; font-size: 11px; margin-bottom: 15px;");

        wineVersionLabel = new QLabel("Wine Version: Checking...");
        wineVersionLabel->setStyleSheet("color: #0f0; font-size: 12px; font-family: 'Nimbus Mono';");

        auto installStableBtn = styledButton(QIcon::fromTheme("system-software-install"), "Install Wine Stable", "#1a3cff");
        auto installStagingBtn = styledButton(QIcon::fromTheme("system-software-install"), "Install Wine Staging (Latest)", "#8e44ad");
        auto updateBtn = styledButton(QIcon::fromTheme("system-software-update"), "Update Wine", "#d35400");

        connect(installStableBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installWineStable);
        connect(installStagingBtn, &QPushButton::clicked, this, &WineOptimizerDialog::installWineStaging);
        connect(updateBtn, &QPushButton::clicked, this, &WineOptimizerDialog::updateWine);

        layout->addWidget(infoLabel);
        layout->addWidget(wineVersionLabel);
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
        infoLabel->setStyleSheet("color: #aaa; font-size: 11px; margin-bottom: 10px;");

        auto presetsGroup = new QGroupBox("Performance Presets");
        presetsGroup->setStyleSheet(groupBoxStyle());
        auto presetsLayout = new QVBoxLayout(presetsGroup);

        auto gamingBtn = styledButton(QIcon::fromTheme("applications-games"), "Gaming Preset (Max Performance)", "#d35400");
        auto balancedBtn = styledButton(QIcon::fromTheme("preferences-system"), "Balanced Preset", "#1a3cff");
        auto compatBtn = styledButton(QIcon::fromTheme("system-run"), "Compatibility Preset", "#8e44ad");

        connect(gamingBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyGamingPreset);
        connect(balancedBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyBalancedPreset);
        connect(compatBtn, &QPushButton::clicked, this, &WineOptimizerDialog::applyCompatPreset);

        presetsLayout->addWidget(gamingBtn);
        presetsLayout->addWidget(balancedBtn);
        presetsLayout->addWidget(compatBtn);

        auto optimGroup = new QGroupBox("Individual Optimizations");
        optimGroup->setStyleSheet(groupBoxStyle());
        auto optimLayout = new QVBoxLayout(optimGroup);

        auto esyncBtn = styledButton(QIcon::fromTheme("media-playback-start"), "Enable ESYNC (Event Synchronization)", "#112266");
        auto fsyncBtn = styledButton(QIcon::fromTheme("media-playback-start"), "Enable FSYNC (Fast Synchronization)", "#112266");
        auto dxvkBtn = styledButton(QIcon::fromTheme("applications-graphics"), "Install DXVK (DirectX to Vulkan)", "#112266");
        auto vkd3dBtn = styledButton(QIcon::fromTheme("applications-graphics"), "Install VKD3D (DirectX 12 to Vulkan)", "#112266");
        auto largeAddrBtn = styledButton(QIcon::fromTheme("edit-find"), "Enable Large Address Aware", "#112266");

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

        layout->addWidget(infoLabel);
        layout->addWidget(presetsGroup);
        layout->addWidget(optimGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createConfigTab() {
        auto widget = new QWidget;
        auto layout = new QVBoxLayout(widget);

        auto infoLabel = new QLabel("Configure Wine prefixes and Windows version emulation.");
        infoLabel->setStyleSheet("color: #aaa; font-size: 11px; margin-bottom: 10px;");

        auto prefixGroup = new QGroupBox("Wine Prefix");
        prefixGroup->setStyleSheet(groupBoxStyle());
        auto prefixLayout = new QVBoxLayout(prefixGroup);

        auto prefixLabel = new QLabel("Select or create a Wine prefix:");
        prefixLabel->setStyleSheet("color: #ccc; font-size: 11px; margin-bottom: 5px;");

        winePrefixCombo = new QComboBox;
        winePrefixCombo->addItem("Default (~/.wine)");
        winePrefixCombo->setEditable(false);
        winePrefixCombo->setStyleSheet("QComboBox { background: #111; color: white; border: 1px solid #333; padding: 5px; }");

        customPrefixEdit = new QLineEdit;
        customPrefixEdit->setPlaceholderText("Or enter custom prefix path...");
        customPrefixEdit->setStyleSheet("QLineEdit { background: #111; color: white; border: 1px solid #333; padding: 5px; }");

        auto createPrefixBtn = styledButton(QIcon::fromTheme("list-add"), "Create New Prefix", "#1a3cff");
        connect(createPrefixBtn, &QPushButton::clicked, this, &WineOptimizerDialog::createPrefix);

        prefixLayout->addWidget(prefixLabel);
        prefixLayout->addWidget(winePrefixCombo);
        prefixLayout->addWidget(customPrefixEdit);
        prefixLayout->addWidget(createPrefixBtn);

        auto winverGroup = new QGroupBox("Windows Version");
        winverGroup->setStyleSheet(groupBoxStyle());
        auto winverLayout = new QVBoxLayout(winverGroup);

        auto winverCombo = new QComboBox;
        winverCombo->addItems({"Windows 10", "Windows 8.1", "Windows 7", "Windows XP"});
        winverCombo->setStyleSheet("QComboBox { background: #111; color: white; border: 1px solid #333; padding: 5px; }");

        auto setWinverBtn = styledButton(QIcon::fromTheme("preferences-desktop"), "Set Windows Version", "#1a3cff");
        connect(setWinverBtn, &QPushButton::clicked, [this, winverCombo]() {
            QString ver = winverCombo->currentText();
            setWindowsVersion(ver);
        });

        winverLayout->addWidget(winverCombo);
        winverLayout->addWidget(setWinverBtn);

        auto toolsGroup = new QGroupBox("Tools");
        toolsGroup->setStyleSheet(groupBoxStyle());
        auto toolsLayout = new QVBoxLayout(toolsGroup);

        auto winecfgBtn = styledButton(QIcon::fromTheme("preferences-system"), "Open Wine Configuration (winecfg)", "#112266");
        auto regeditBtn = styledButton(QIcon::fromTheme("edit-find-replace"), "Open Wine Registry Editor", "#112266");
        auto taskmgrBtn = styledButton(QIcon::fromTheme("utilities-system-monitor"), "Open Wine Task Manager", "#112266");

        connect(winecfgBtn, &QPushButton::clicked, []() { QProcess::startDetached("winecfg"); });
        connect(regeditBtn, &QPushButton::clicked, []() { QProcess::startDetached("wine", {"regedit"}); });
        connect(taskmgrBtn, &QPushButton::clicked, []() { QProcess::startDetached("wine", {"taskmgr"}); });

        toolsLayout->addWidget(winecfgBtn);
        toolsLayout->addWidget(regeditBtn);
        toolsLayout->addWidget(taskmgrBtn);

        layout->addWidget(infoLabel);
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
        infoLabel->setStyleSheet("color: #aaa; font-size: 11px; margin-bottom: 15px;");

        auto cacheBtn = styledButton(QIcon::fromTheme("edit-clear"), "Clear Wine Cache", "#d35400");
        auto tempBtn = styledButton(QIcon::fromTheme("edit-clear"), "Clean Temp Files", "#d35400");
        auto prefixBtn = styledButton(QIcon::fromTheme("user-trash"), "Remove Unused Prefixes", "#d35400");
        auto fullCleanBtn = styledButton(QIcon::fromTheme("edit-clear-all"), "Full Cleanup (All Above)", "#c0392b");

        connect(cacheBtn, &QPushButton::clicked, this, &WineOptimizerDialog::clearCache);
        connect(tempBtn, &QPushButton::clicked, this, &WineOptimizerDialog::clearTemp);
        connect(prefixBtn, &QPushButton::clicked, this, &WineOptimizerDialog::cleanPrefixes);
        connect(fullCleanBtn, &QPushButton::clicked, this, &WineOptimizerDialog::fullCleanup);

        layout->addWidget(infoLabel);
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
            statusLabel->setStyleSheet("color: #f00;");
            wineVersionLabel->setText("Wine Version: Not Installed");
            logArea->append("Wine is not installed on this system.");
        } else {
            statusLabel->setText(QString("Wine installed: %1").arg(version));
            statusLabel->setStyleSheet("color: #0f0;");
            wineVersionLabel->setText(QString("Wine Version: %1").arg(version));
            logArea->append(QString("Wine detected: %1").arg(version));
        }
    }

    void installWineStable() {
        logArea->append("\nInstalling Wine Stable...");
        QString cmd = "dpkg --add-architecture i386 && apt update && "
                      "apt install -y wine wine32 wine64 libwine libwine:i386 fonts-wine";
        runSudoInTerminal(cmd, this, "Installing Wine Stable");
        QTimer::singleShot(3000, this, &WineOptimizerDialog::checkWineInstallation);
    }

    void installWineStaging() {
        logArea->append("\nInstalling Wine Staging...");
        QString cmd = "dpkg --add-architecture i386 && "
                      "wget -nc https://dl.winehq.org/wine-builds/winehq.key && "
                      "apt-key add winehq.key && "
                      "add-apt-repository 'deb https://dl.winehq.org/wine-builds/debian/ bookworm main' && "
                      "apt update && apt install -y --install-recommends winehq-staging";
        runSudoInTerminal(cmd, this, "Installing Wine Staging");
        QTimer::singleShot(3000, this, &WineOptimizerDialog::checkWineInstallation);
    }

    void updateWine() {
        logArea->append("\nUpdating Wine...");
        runSudoInTerminal("apt update && apt upgrade -y wine* winehq*", this, "Updating Wine");
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
        QString cmd = "apt install -y dxvk";
        runSudoInTerminal(cmd, this, "Installing DXVK");
    }

    void installVKD3D() {
        logArea->append("\nInstalling VKD3D...");
        QString cmd = "apt install -y vkd3d-compiler libvkd3d1 libvkd3d-dev";
        runSudoInTerminal(cmd, this, "Installing VKD3D");
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
    QString groupBoxStyle() const {
        return "QGroupBox { color: #ffffff; font-weight: bold; font-size: 13px; margin-top: 12px;"
               "padding-top: 12px; background: #0a0a0a; border: 1px solid #1a3cff; border-radius: 6px; }"
               "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 8px; color: #00bfff; }";
    }

    QPushButton* styledButton(const QIcon &icon, const QString &text, const QString &color = "#112266") {
        auto btn = new QPushButton(icon, text);
        btn->setStyleSheet(QString(
                               "QPushButton { background: %1; color: white; border: 1px solid #5a6fff; "
                               "padding: 10px 16px; border-radius: 5px; font-weight: bold; font-size: 12px; }"
                               "QPushButton:hover { background: #1a3cff; border: 2px solid #00bfff; }"
                               "QPushButton:pressed { background: #0a2cff; }"
                               ).arg(color));
        return btn;
    }
};
// --- Settings Panel (Launcher) ---
class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    SettingsPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto layout = new QVBoxLayout(this);

        auto iconLabel = new QLabel;
        iconLabel->setPixmap(QPixmap(":/txtlogo.svgz").scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignCenter);

        auto infoLabel = new QLabel("version 2.0.6 (for neospace 2025)");
        infoLabel->setStyleSheet("color: white; margin-bottom: 10px; font-family:  'Nimbus Mono', 'Nimbus Mono';");

        auto wineBtn = new QPushButton(QIcon::fromTheme("wine", QIcon::fromTheme("application-x-executable")), "Wine Optimizer");
        wineBtn->setStyleSheet(
            "QPushButton { background-color: #112266; color: white; border: none; padding: 12px; border-radius: 5px; font-weight: bold; font-family: 'Nimbus Mono'; }"
            "QPushButton:hover { background-color: #1a3cff;  }"
            );
        connect(wineBtn, &QPushButton::clicked, this, [this]() {
            WineOptimizerDialog dialog(this);
            dialog.exec();
        });

        auto openBtn = new QPushButton(QIcon::fromTheme("settings"), "Open Settings");
        openBtn->setStyleSheet(
            "QPushButton { background-color: #112266; color: white; border: none; padding: 12px; border-radius: 5px; font-weight: bold; font-family: 'Nimbus Mono'; }"
            "QPushButton:hover { background-color: #1a3cff; }"
            );

        auto githubBtn = new QPushButton(QIcon::fromTheme("url-copy"), "Check in GitHub");
        githubBtn->setStyleSheet(
            "QPushButton { background-color: #222; color: #00BFFF; border: 1px solid #00BFFF; padding: 8px; border-radius: 4px; font-family: 'Nimbus Mono'; }"
            "QPushButton:hover { background-color: #00BFFF; color: black; }"
            );

        auto wallpBtn = new QPushButton(QIcon::fromTheme("preferences-wallpaper"), "Check out more wallpapers for error.os");
        wallpBtn->setStyleSheet(
            "QPushButton { background-color: #222; color: #00BFFF; border: 1px solid #00BFFF; padding: 8px; border-radius: 4px; font-family: 'Nimbus Mono'; }"
            "QPushButton:hover { background-color: #00BFFF; color: black; }"
            );

        auto versionLabel = new QLabel("err_ v2.0.6 — error.dashboard for neospace");
        versionLabel->setStyleSheet("color: #AAAAAA; font-size: 12px; font-family: 'Nimbus Mono'; margin-top: 10px;");
        versionLabel->setAlignment(Qt::AlignCenter);

        auto descLabel = new QLabel("this application is made for troubleshooting error.os's barebones issues");
        descLabel->setStyleSheet("color: #CCCCCC; font-size: 11px; font-family: 'Nimbus Mono'; margin-bottom: 10px;");
        descLabel->setWordWrap(true);
        descLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(iconLabel);
        layout->addWidget(infoLabel);
        layout->addWidget(wineBtn);
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
            QDesktopServices::openUrl(QUrl("https://zynomon.github.io/errpaper"));
        });
    }};

// --- Main Window ---
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("err_ - Your home");
        setWindowIcon(QIcon::fromTheme("err_"));
        setMinimumSize(800, 540);
        resize(900, 600);

        setStyleSheet("QMainWindow { background: rgba(10, 10, 10, 220); }");

        QWidget *mainWidget = new QWidget(this);
        mainWidget->setStyleSheet("background: transparent;");
        setCentralWidget(mainWidget);

        QVBoxLayout *layout = new QVBoxLayout(mainWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QTabWidget *tabWidget = new QTabWidget(this);

        tabWidget->setStyleSheet(
            "QTabWidget::pane { "
            "   border: 1px solid #2a3245; "
            "   background-color: #0d0d0d; "
            "   border-radius: 6px; "
            "   padding: 4px; "
            "} "
            "QTabBar::tab { "
            "   background-color: #1a1a1a; "
            "   color: #9ca0b0; "
            "   padding: 8px 20px; "
            "   font-family: 'Nimbus Mono'; "
            "   font-size: 11pt; "
            "   border-top-left-radius: 6px; "
            "   border-top-right-radius: 6px; "
            "   margin: 2px; "
            "} "
            "QTabBar::tab:selected { "
            "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #6c7fff, stop:1 #5a6fff); "
            "   color: #000000; "
            "   font-weight: bold; "
            "   border: 1px solid #5a6fff; "
            "} "
            "QTabBar::tab:!selected:hover { "
            "   background-color: #2a3245; "
            "   color: #ffffff; "
            "   border: 1px solid #5a6fff; "
            "   font-weight: bold; "
            "} "
            );

        tabWidget->addTab(new SystemInfoPanel(), QIcon::fromTheme("system-help"), "System Info");
        tabWidget->addTab(new DriverManager(), QIcon::fromTheme("driver-manager"), "Drivers");
        tabWidget->addTab(new AppInstaller(), QIcon::fromTheme("system-installer"), "Install Apps");
        tabWidget->addTab(new AppRemover(), QIcon::fromTheme("trashcan_empty"), "Remove Apps");
        tabWidget->addTab(new SettingsPanel(), QIcon::fromTheme("preferences-wallpaper"), "Extra Settings");

        layout->addWidget(tabWidget);
    }};

#include "main.moc"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("err_");
    app.setApplicationVersion("2.0.6");
    app.setOrganizationName("error.os");

    QFont mono("Nimbus Mono");
    if (!mono.exactMatch()) mono = QFont("monospace");
    mono.setStyleHint(QFont::Monospace);
    app.setFont(mono);

    MainWindow window;
    window.show();

    return app.exec();
}
