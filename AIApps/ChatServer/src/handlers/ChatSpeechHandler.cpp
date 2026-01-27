#include "../include/handlers/ChatSpeechHandler.h"
#include <muduo/base/Logging.h>


void ChatSpeechHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        LOG_INFO << "[ChatSpeechHandler] /chat/tts 请求开始处理";

        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "[ChatSpeechHandler] session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {

            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(),
                errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");
        LOG_INFO << "[ChatSpeechHandler] 当前用户 userId=" << userId << ", username=" << username;


        std::string text;

        auto body = req.getBody();
        LOG_INFO << "[ChatSpeechHandler] 收到请求体: " << body;
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("text")) text = j["text"];
        }
        LOG_INFO << "[ChatSpeechHandler] 准备合成的文本内容长度: " << text.size();


        const char* secretEnv = std::getenv("BAIDU_CLIENT_SECRET");
        const char* idEnv = std::getenv("BAIDU_CLIENT_ID");
        LOG_INFO << "[ChatSpeechHandler] 读取环境变量 BAIDU_CLIENT_SECRET 和 BAIDU_CLIENT_ID";

        if (!secretEnv) {
            LOG_ERROR << "[ChatSpeechHandler] BAIDU_CLIENT_SECRET not found!";
            throw std::runtime_error("BAIDU_CLIENT_SECRET not found!");
        }
        if (!idEnv) {
            LOG_ERROR << "[ChatSpeechHandler] BAIDU_CLIENT_ID not found!";
            throw std::runtime_error("BAIDU_CLIENT_ID not found!");
        }

        std::string clientSecret(secretEnv);
        std::string clientId(idEnv);
        LOG_INFO << "[ChatSpeechHandler] 成功获取百度语音 clientId 与 clientSecret，开始创建 AISpeechProcessor 实例";

        AISpeechProcessor speechProcessor(clientId, clientSecret);
        

        LOG_INFO << "[ChatSpeechHandler] 调用 AISpeechProcessor::synthesize 开始，文本长度: " << text.size();
        std::string speechUrl = speechProcessor.synthesize(text,
                                                           "mp3-16k", 
                                                           "zh",  
                                                            5, 
                                                            5, 
                                                            5 );  
        LOG_INFO << "[ChatSpeechHandler] 调用 AISpeechProcessor::synthesize 结束，返回 url: " << speechUrl;

        json successResp;
        successResp["success"] = true;
        successResp["url"] = speechUrl;
        std::string successBody = successResp.dump(4);
        LOG_INFO << "[ChatSpeechHandler] 返回成功响应，body=" << successBody;
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[ChatSpeechHandler] 处理 /chat/tts 请求异常: " << e.what();
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}









