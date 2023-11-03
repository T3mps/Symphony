#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <source_location>

namespace Symphony
{
   enum class LogLevel
   {
      Trace,
      Debug,
      Info,
      Warn,
      Error,
      Fatal
   };

   class ILogger
   {
   public:
      virtual ~ILogger() = default;
      virtual void Log(LogLevel level, std::string_view message, const std::source_location& location = std::source_location::current()) = 0;
   };

   class Logger : public ILogger
   {
   public:
      void Log(LogLevel level, std::string_view message, const std::source_location& location = std::source_location::current()) override
      {
         std::cout << "[" << ToString(level) << "]["
            << location.file_name() << ":" << location.line() << "] - "
            << message << std::endl;
      }

   private:
      std::string ToString(LogLevel level)
      {
         switch (level)
         {
         case LogLevel::Trace: return "TRACE";
         case LogLevel::Debug: return "DEBUG";
         case LogLevel::Info:  return "INFO";
         case LogLevel::Warn:  return "WARN";
         case LogLevel::Error: return "ERROR";
         case LogLevel::Fatal: return "FATAL";
         default:              return "UNKNOWN";
         }
      }
   };

   class LogManager
   {
   public:
      static void SetLogger(ILogger* newLogger)
      {
         GetInstance().m_logger = newLogger;
      }

      static ILogger& GetLogger()
      {
         return *(GetInstance().m_logger);
      }

   private:
      LogManager() : m_logger(new Logger())
      {}

      ~LogManager()
      {
         delete m_logger;
      }

      LogManager(const LogManager&) = delete;
      LogManager& operator=(const LogManager&) = delete;

      static LogManager& GetInstance()
      {
         static LogManager logManagerInstance;
         return logManagerInstance;
      }

      ILogger* m_logger;
   };
}

#define LOG_TRACE(message) LogManager::GetLogger().Log(LogLevel::Trace, message, std::source_location::current())
#define LOG_DEBUG(message) LogManager::GetLogger().Log(LogLevel::Debug, message, std::source_location::current())
#define LOG_INFO(message)  LogManager::GetLogger().Log(LogLevel::Info,  message, std::source_location::current())
#define LOG_WARN(message)  LogManager::GetLogger().Log(LogLevel::Warn,  message, std::source_location::current())
#define LOG_ERROR(message) LogManager::GetLogger().Log(LogLevel::Error, message, std::source_location::current())
#define LOG_FATAL(message) LogManager::GetLogger().Log(LogLevel::Fatal, message, std::source_location::current())