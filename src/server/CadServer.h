#pragma once

// C/S 服务端（阶段 5）：基于 TCP 的 JSON 协议服务，管理一个 CadDocument。
// 协议：每行一条 JSON 请求（以 \n 结束），返回一行 JSON 响应。
// 支持的命令：ping / count / list / get / add / update / remove / clear /
//             save / load / exportDxf / measure / tangent
#include <QObject>

class CadDocument;
class QTcpServer;
class QTcpSocket;
class QJsonObject;

class CadServer : public QObject
{
    Q_OBJECT

public:
    explicit CadServer(QObject* parent = nullptr);
    ~CadServer();

    void setDocument(CadDocument* doc) { m_doc = doc; }
    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const { return m_port; }

    // 核心协议处理（不依赖 socket，可单独单元测试）
    QJsonObject handleRequest(const QJsonObject& req);

signals:
    void logMessage(const QString& text);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QTcpServer*          m_tcp = nullptr;
    CadDocument*         m_doc = nullptr;
    quint16              m_port = 0;
    QMap<QTcpSocket*, QByteArray> m_buffer;
};
