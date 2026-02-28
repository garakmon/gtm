#pragma once
#ifndef LOGGING_H
#define LOGGING_H

#include <QSet>
#include <QString>

#include <cstdint>



namespace logging {

enum class LogLevel {
    Debug = 0,
    Info,
    Warn,
    Error,
};

enum class LogCategory : unsigned {
    General = 1 << 0,
    FileIO  = 1 << 1,
    Audio   = 1 << 2,
    Project = 1 << 3,
    Ui      = 1 << 4,
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Logger stores the current console logging policy for the application.
/// It owns the level, category filtering, debug-session state, and repeated
/// info-message suppression state used by the logging namespace wrappers.
///
/// It is possible to modify the output of what gets logged, and therefore the state
/// needs to be stored
///
//////////////////////////////////////////////////////////////////////////////////////////
class Logger {
public:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger &operator=(Logger &&) = delete;

    void setLevel(LogLevel level);
    LogLevel level() const;

    void setCategoryEnabled(LogCategory category, bool enabled);
    bool isCategoryEnabled(LogCategory category) const;

    void setDebugging(bool enabled);
    bool isDebugSession() const;

    void setDeduplicateInfo(bool enabled);
    bool deduplicateInfo() const;

    void debug(const QString &message, LogCategory category = LogCategory::General);
    void info(const QString &message, LogCategory category = LogCategory::General);
    void warn(const QString &message, LogCategory category = LogCategory::General);
    void error(const QString &message, LogCategory category = LogCategory::General);

private:
    void log(LogLevel level, const QString &message, LogCategory category);
    bool shouldLog(LogLevel level, LogCategory category, const QString &message);

private:
    LogLevel m_level = LogLevel::Info;
    uint32_t m_enabled_categories = 0xffffffffu;
    bool m_debug_session = false;
    bool m_deduplicate_info = true;
    QSet<QString> m_seen_info_messages;
};

void setLevel(LogLevel level);
LogLevel level();

void setCategoryEnabled(LogCategory category, bool enabled);
bool isCategoryEnabled(LogCategory category);

void setDebugging(bool enabled);
bool isDebugSession();

void setDeduplicateInfo(bool enabled);
bool deduplicateInfo();

void debug(const QString &message, LogCategory category = LogCategory::General);
void info(const QString &message, LogCategory category = LogCategory::General);
void warn(const QString &message, LogCategory category = LogCategory::General);
void error(const QString &message, LogCategory category = LogCategory::General);

} // namespace logging

#endif // LOGGING_H
