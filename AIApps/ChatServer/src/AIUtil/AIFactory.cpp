#include"../include/AIUtil/AIFactory.h"
#include <muduo/base/Logging.h>


StrategyFactory& StrategyFactory::instance() {
    static StrategyFactory factory;
    return factory;
}

void StrategyFactory::registerStrategy(const std::string& name, Creator creator) {
    creators[name] = std::move(creator);
}

std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string& name) {
    LOG_INFO << "[StrategyFactory] 开始创建策略，名称: " << name << "，位置: AIFactory.cpp create方法";
    std::cout << "[StrategyFactory] 创建策略，名称: " << name << "，位置: AIFactory.cpp create方法" << std::endl;
    auto it = creators.find(name);
    if (it == creators.end()) {
        LOG_ERROR << "[StrategyFactory] 错误：未知策略 " << name << "，位置: AIFactory.cpp create方法";
        std::cerr << "[StrategyFactory] 错误：未知策略 " << name << "，位置: AIFactory.cpp create方法" << std::endl;
        throw std::runtime_error("Unknown strategy: " + name);
    }
    try {
        LOG_INFO << "[StrategyFactory] 找到策略注册，准备创建策略实例: " << name << "，位置: AIFactory.cpp create方法";
        std::cout << "[StrategyFactory] 正在创建策略实例: " << name << std::endl;
        LOG_INFO << "[StrategyFactory] 调用策略构造函数，策略名称: " << name << "，位置: AIFactory.cpp create方法";
        auto strategy = it->second();
        LOG_INFO << "[StrategyFactory] 策略创建成功: " << name << "，位置: AIFactory.cpp create方法";
        std::cout << "[StrategyFactory] 策略创建成功: " << name << std::endl;
        return strategy;
    } catch (const std::exception& e) {
        LOG_ERROR << "[StrategyFactory] 创建策略异常: " << name << "，异常信息: " << e.what() << "，位置: AIFactory.cpp create方法";
        std::cerr << "[StrategyFactory] 创建策略异常: " << name << "，异常: " << e.what() << "，位置: AIFactory.cpp create方法" << std::endl;
        throw;
    }
}
