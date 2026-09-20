#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "models/radiostate.h"
#include "models/menumodel.h"
#include "network/tcpclient.h"

// Exercise the production TCP framing and I/O-thread dispatch. The fake K4
// deliberately sends no SET echo; state replies are withheld until requested.
class MacroRadio : public QObject {
public:
    QTcpServer server;
    QThread io;
    TcpClient *client = new TcpClient;
    QTcpSocket *peer = nullptr;
    Protocol parser;
    RadioState state;
    MenuModel menus;
    QStringList packets;

    MacroRadio() {
        connect(client->protocol(), &Protocol::catResponseReceived, &state, [this](const QString &response) {
            for (const auto &command : response.split(';', Qt::SkipEmptyParts)) {
                state.parseCATCommand(command + ';');
                if (command.startsWith("MEDF")) menus.parseMEDF(command + ';');
                else if (command.startsWith("ME")) menus.parseME(command + ';');
            }
        });
        connect(&parser, &Protocol::catResponseReceived, this, [this](const QString &packet) {
            packets.append(packet);
        });
        connect(&server, &QTcpServer::newConnection, this, [this] {
            peer = server.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, this, [this] { parser.parse(peer->readAll()); });
            peer->write(Protocol::buildCATPacket("FA00014074000;FB00007074000;"));
        });
        client->moveToThread(&io);
        io.start();
    }

    ~MacroRadio() override {
        QMetaObject::invokeMethod(client, [this] { client->disconnectFromHost(); delete client; },
                                  Qt::BlockingQueuedConnection);
        io.quit();
        io.wait();
    }

    bool start() {
        if (!server.listen(QHostAddress::LocalHost))
            return false;
        const quint16 port = server.serverPort();
        QMetaObject::invokeMethod(client, [this, port] { client->connectToHost("127.0.0.1", port, "test"); });
        return true;
    }

    void barrier() {
        // A read-only sentinel sent after all queued macro operations.
        client->sendCAT("ID;");
    }
};

class MacroTest : public QObject {
    Q_OBJECT
private slots:
    void frequencyReadback_data() {
        QTest::addColumn<QString>("macro");
        QTest::addColumn<quint64>("frequencyA");
        QTest::addColumn<quint64>("frequencyB");
        QTest::newRow("issue-2-short-khz") << QString("FA3702;") << quint64(3702000) << quint64(7074000);
        QTest::newRow("full-hz") << QString("FA00003702000;") << quint64(3702000) << quint64(7074000);
        QTest::newRow("vfo-b") << QString("FB10136;") << quint64(14074000) << quint64(10136000);
        QTest::newRow("both-vfos") << QString("FA3702;FB5357;") << quint64(3702000) << quint64(5357000);
        QTest::newRow("swap") << QString("AB2;") << quint64(7074000) << quint64(14074000);
        QTest::newRow("band") << QString("BN+;") << quint64(18100000) << quint64(7074000);
        QTest::newRow("radio-rejects-set") << QString("FA99999999999;") << quint64(14074000) << quint64(7074000);
        QTest::newRow("text-keeps-case-and-spaces") << QString("KY CQ de AE6LX;") << quint64(14074000) << quint64(7074000);
        QTest::newRow("existing-readback") << QString("FA3702;FA;") << quint64(3702000) << quint64(7074000);
    }

    void frequencyReadback() {
        QFETCH(QString, macro);
        QFETCH(quint64, frequencyA);
        QFETCH(quint64, frequencyB);
        MacroRadio radio;
        QVERIFY(radio.start());
        QTRY_VERIFY(radio.packets.contains("SL3;")); // Initial connection sequence has drained.
        QTRY_COMPARE(radio.state.vfoA(), quint64(14074000));
        QTRY_COMPARE(radio.state.vfoB(), quint64(7074000));
        radio.packets.clear();
        QSignalSpy frequencySpy(&radio.state, &RadioState::frequencyChanged);

        radio.client->sendMacro(macro);
        radio.barrier();
        QTRY_VERIFY(radio.packets.contains("ID;"));
        radio.packets.removeAll("PING;");
        QCOMPARE(radio.packets, QStringList({macro, "RDY;",
            "#DSM;#HDSM;#PKM;#AR;#NB$;#NBL$;#FRZ;#FPS;#SCL;RT$;RO$;VT;VT$;KP;PL;PL$;RP;", "ID;"}));
        QCOMPARE(frequencySpy.count(), 0);
        QCOMPARE(radio.state.vfoA(), quint64(14074000));
        QCOMPARE(radio.state.vfoB(), quint64(7074000));

        // Only actual radio replies update the model and its display signal.
        radio.peer->write(Protocol::buildCATPacket(QString("FA%1;FB%2;")
            .arg(frequencyA, 11, 10, QChar('0')).arg(frequencyB, 11, 10, QChar('0'))));
        QTRY_COMPARE(frequencySpy.count(), 1);
        QCOMPARE(frequencySpy.first().first().toULongLong(), frequencyA);
        QCOMPARE(radio.state.vfoA(), frequencyA);
        QCOMPARE(radio.state.vfoB(), frequencyB);
    }

    void emptyAndDisconnectedMacrosDoNothing() {
        MacroRadio radio;
        radio.client->sendMacro("FA3702;"); // Disconnected: must not replay on connection.
        QVERIFY(radio.start());
        QTRY_VERIFY(radio.packets.contains("SL3;"));
        QVERIFY(!radio.packets.contains("FA3702;"));
        QCOMPARE(radio.packets.count("RDY;"), 1); // Connection only, no deferred macro refresh.
        radio.packets.clear();
        radio.client->sendMacro("");
        radio.client->sendMacro("  \r\n");
        radio.barrier();
        QTRY_VERIFY(radio.packets.contains("ID;"));
        radio.packets.removeAll("PING;");
        QCOMPARE(radio.packets, QStringList({"ID;"}));
    }

    void ordinaryCatDoesNotAddQueries() {
        MacroRadio radio;
        QVERIFY(radio.start());
        QTRY_VERIFY(radio.packets.contains("SL3;"));
        radio.packets.clear();
        radio.client->sendCAT("FA3702;");
        radio.barrier();
        QTRY_VERIFY(radio.packets.contains("ID;"));
        radio.packets.removeAll("PING;");
        QCOMPARE(radio.packets, QStringList({"FA3702;", "ID;"}));
    }

    void fullStateRefreshIncludesFiltersAndMenus() {
        MacroRadio radio;
        QVERIFY(radio.start());
        QTRY_VERIFY(radio.packets.contains("SL3;"));
        radio.packets.clear();
        QSignalSpy bandwidthSpy(&radio.state, &RadioState::filterBandwidthChanged);
        QSignalSpy subBandwidthSpy(&radio.state, &RadioState::filterBandwidthBChanged);
        QSignalSpy modeSpy(&radio.state, &RadioState::modeChanged);
        QSignalSpy freezeSpy(&radio.state, &RadioState::freezeChanged);
        const QString macro = "MD3;FP2;BW0050;IS+0010;MD$2;BW$0240;PC025H;RT/;#FRZ/;ME0007.0123;";
        radio.client->sendMacro(macro);
        radio.barrier();
        QTRY_VERIFY(radio.packets.contains("ID;"));
        QCOMPARE(radio.packets.count(macro), 1);
        QCOMPARE(radio.packets.count("RDY;"), 1);
        QCOMPARE(bandwidthSpy.count(), 0);
        QCOMPARE(modeSpy.count(), 0);

        const QString dump = "FA00014074000;FB00007074000;MD3;FP2;BW0050;IS+0010;"
                             "MD$2;BW$0240;PC025H;RT1;"
                             "MEDF0007,AGC Hold Time,RX AGC,DEC,1,0,200,0,123,1;";
        radio.peer->write(Protocol::buildCATPacket(dump));
        // The supplemental query handles a display setting absent from RDY.
        radio.peer->write(Protocol::buildCATPacket("#FRZ1;VT2;VT$3;"));
        QTRY_COMPARE(freezeSpy.count(), 1);
        QCOMPARE(radio.state.mode(), RadioState::CW);
        QCOMPARE(radio.state.modeB(), RadioState::USB);
        QCOMPARE(radio.state.filterPosition(), 2);
        QCOMPARE(radio.state.filterBandwidth(), 500);
        QCOMPARE(radio.state.filterBandwidthB(), 2400);
        QCOMPARE(radio.state.shiftHz(), 100);
        QCOMPARE(radio.state.rfPower(), 25.0);
        QVERIFY(radio.state.ritEnabled());
        QVERIFY(radio.state.freeze());
        QCOMPARE(radio.state.tuningStep(), 2);
        QCOMPARE(radio.state.tuningStepB(), 3);
        QCOMPARE(bandwidthSpy.count(), 1);
        QCOMPARE(subBandwidthSpy.count(), 1);
        QCOMPARE(modeSpy.count(), 1);
        QVERIFY(radio.menus.getMenuItem(7));
        QCOMPARE(radio.menus.getMenuItem(7)->currentValue, 123);

        // Repeated macro refreshes must replace menu definitions, not add duplicates.
        radio.packets.clear();
        radio.client->sendMacro("BW0050;");
        radio.barrier();
        QTRY_VERIFY(radio.packets.contains("ID;"));
        QCOMPARE(radio.packets.count("RDY;"), 1);
        QSignalSpy menuSpy(&radio.menus, &MenuModel::menuItemAdded);
        radio.peer->write(Protocol::buildCATPacket(dump));
        QTRY_COMPARE(menuSpy.count(), 1);
        QCOMPARE(radio.menus.getAllItems().size(), 1);
        QCOMPARE(bandwidthSpy.count(), 1); // Unchanged value does not retrigger controls.
    }
};

QTEST_GUILESS_MAIN(MacroTest)
#include "test_macros.moc"
