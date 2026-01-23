#include "../include/handlers/ChatCreateAndSendHandler.h"
#include <muduo/base/Logging.h>
#include <thread>
#include <sstream>


void ChatCreateAndSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto tid = std::this_thread::get_id();
    std::string tidStr;
    {
        std::ostringstream oss;
        oss << tid;
        tidStr = oss.str();
    }
    try
    {
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] /chat/create_and_send 开始处理";
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] start /chat/create_and_send" << std::endl;

        auto safeSnippet = [](const std::string& s, size_t maxLen = 512) {
            if (s.size() <= maxLen) return s;
            return s.substr(0, maxLen) + "...(truncated)";
        };

        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 获取 session 中";
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] acquiring session" << std::endl;
        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] session isLoggedIn=" << session->getValue("isLoggedIn");
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
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 当前用户 userId=" << userId << ", username=" << username;
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] userId=" << userId << ", username=" << username << std::endl;

        std::string userQuestion;
        std::string modelType;

        auto body = req.getBody();
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 收到请求体长度=" << body.size();
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 请求体内容片段=" << safeSnippet(body, 256);
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] bodyLen=" << body.size() << std::endl;
        try {
            if (!body.empty()) {
                auto j = json::parse(body);
                if (j.contains("question")) userQuestion = j["question"];
                modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
            }
        } catch (const std::exception& ex) {
            LOG_ERROR << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 解析请求体异常: " << ex.what();
            std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] parse body failed: " << ex.what() << std::endl;
            throw;
        }
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 解析到 modelType=" << modelType
                 << ", questionLength=" << userQuestion.size()
                 << ", question片段=" << safeSnippet(userQuestion, 200);
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] parsed modelType=" << modelType
                  << ", qLen=" << userQuestion.size() << std::endl;

        AISessionIdGenerator generator;
        std::string sessionId = generator.generate();
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 生成 sessionId=" << sessionId;
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] sessionId=" << sessionId << std::endl;


        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);

            auto& userSessions = server_->chatInformation[userId];
            LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] userSessions size=" << userSessions.size();
            std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] userSessions size=" << userSessions.size() << std::endl;

            if (userSessions.find(sessionId) == userSessions.end()) {

                userSessions.emplace(
                    sessionId,
                    std::make_shared<AIHelper>()
                );
                server_->sessionsIdsMap[userId].push_back(sessionId);
                LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 创建新的 AIHelper, sessionsIdsMap size=" << server_->sessionsIdsMap[userId].size();
                std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] new AIHelper created" << std::endl;
            }
            AIHelperPtr= userSessions[sessionId];

        }

        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 调用 AIHelper::chat，sessionId=" << sessionId;
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] calling AIHelper::chat" << std::endl;
        std::string aiInformation;
        try {
            aiInformation = AIHelperPtr->chat(userId, username,sessionId, userQuestion, modelType);
        } catch (const std::exception& ex) {
            LOG_ERROR << "[ChatCreateAndSendHandler][tid=" << tidStr << "] AIHelper::chat 抛出异常: " << ex.what();
            std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] AIHelper::chat error: " << ex.what() << std::endl;
            throw;
        }
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] AIHelper::chat 返回信息长度=" << aiInformation.size()
                 << ", 返回片段=" << safeSnippet(aiInformation, 200);
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] AIHelper::chat done, infoLen=" << aiInformation.size() << std::endl;
        json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        successResp["sessionId"] = sessionId;
        
        std::string successBody = successResp.dump(4);
        LOG_INFO << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 返回成功响应，body长度=" << successBody.size();
        std::cerr << "[ChatCreateAndSendHandler][tid=" << tidStr << "] respond success, bodyLen=" << successBody.size() << std::endl;

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[ChatCreateAndSendHandler][tid=" << tidStr << "] 处理 /chat/create_and_send 异常: " << e.what();
        std::cerr << "[ChatCreateAndSendHandler][ERROR][tid=" << tidStr << "] " << e.what() << std::endl;

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









