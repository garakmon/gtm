#include "util/logging.h"

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>



namespace {

// This mutex exists because we may log (or change log state) between ui and core threads.
QMutex s_logger_mutex;
logging::Logger s_logger;

} // namespace



static uint32_t categoryMask(logging::LogCategory category) {
    return static_cast<uint32_t>(category);
}

static QString categoryName(logging::LogCategory category) {
    switch (category) {
    case logging::LogCategory::General:
        return "general";
    case logging::LogCategory::FileIO:
        return "file i/o";
    case logging::LogCategory::Audio:
        return "audio";
    case logging::LogCategory::Project:
        return "project";
    case logging::LogCategory::Ui:
        return "ui";
    }

    return "unknown";
}

static QString categoryColor(logging::LogCategory category) {
    switch (category) {
    case logging::LogCategory::General:
        return "\x1b[37m";
    case logging::LogCategory::FileIO:
        return "\x1b[37m";
    case logging::LogCategory::Audio:
        return "\x1b[34m";
    case logging::LogCategory::Project:
        return "\x1b[34m";
    case logging::LogCategory::Ui:
        return "\x1b[34m";
    }

    return "\x1b[37m";
}

static QString levelName(logging::LogLevel level) {
    switch (level) {
    case logging::LogLevel::Debug:
        return "debug";
    case logging::LogLevel::Info:
        return "info";
    case logging::LogLevel::Warn:
        return "warn";
    case logging::LogLevel::Error:
        return "error";
    }

    return "log";
}



void logging::Logger::setLevel(LogLevel level) {
    m_level = level;
}

logging::LogLevel logging::Logger::level() const {
    return m_level;
}

void logging::Logger::setCategoryEnabled(LogCategory category, bool enabled) {
    const uint32_t mask = categoryMask(category);
    if (enabled) {
        m_enabled_categories |= mask;
        return;
    }

    m_enabled_categories &= ~mask;
}

bool logging::Logger::isCategoryEnabled(LogCategory category) const {
    return (m_enabled_categories & categoryMask(category)) != 0;
}

void logging::Logger::setDebugging(bool enabled) {
    m_debug_session = enabled;
}

bool logging::Logger::isDebugSession() const {
    return m_debug_session;
}

void logging::Logger::setDeduplicateInfo(bool enabled) {
    m_deduplicate_info = enabled;
    if (!enabled) {
        m_seen_info_messages.clear();
    }
}

bool logging::Logger::deduplicateInfo() const {
    return m_deduplicate_info;
}

void logging::Logger::debug(const QString &message, LogCategory category) {
    this->log(LogLevel::Debug, message, category);
}

void logging::Logger::info(const QString &message, LogCategory category) {
    this->log(LogLevel::Info, message, category);
}

void logging::Logger::warn(const QString &message, LogCategory category) {
    this->log(LogLevel::Warn, message, category);
}

void logging::Logger::error(const QString &message, LogCategory category) {
    this->log(LogLevel::Error, message, category);
}

void logging::Logger::log(LogLevel level, const QString &message, LogCategory category) {
    if (!this->shouldLog(level, category, message)) {
        return;
    }

    const QString prefix = QString("%1[%2][%3]\x1b[0m")
                               .arg(categoryColor(category))
                               .arg(levelName(level))
                               .arg(categoryName(category));
    switch (level) {
    case LogLevel::Debug:
    case LogLevel::Info:
        qDebug().noquote() << prefix << message;
        return;
    case LogLevel::Warn:
        qWarning().noquote() << prefix << message;
        return;
    case LogLevel::Error:
        qCritical().noquote() << prefix << message;
        return;
    }
}

bool logging::Logger::shouldLog(LogLevel level, LogCategory category, const QString &message) {
    if (!this->isCategoryEnabled(category)) {
        return false;
    }

    if (level == LogLevel::Debug && !m_debug_session) {
        return false;
    }

    if (static_cast<int>(level) < static_cast<int>(m_level)) {
        return false;
    }

    if (level != LogLevel::Info || !m_deduplicate_info) {
        return true;
    }

    if (m_seen_info_messages.contains(message)) {
        return false;
    }

    m_seen_info_messages.insert(message);
    return true;
}

void logging::setLevel(LogLevel level) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.setLevel(level);
}

logging::LogLevel logging::level() {
    QMutexLocker lock(&s_logger_mutex);
    return s_logger.level();
}

void logging::setCategoryEnabled(LogCategory category, bool enabled) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.setCategoryEnabled(category, enabled);
}

bool logging::isCategoryEnabled(LogCategory category) {
    QMutexLocker lock(&s_logger_mutex);
    return s_logger.isCategoryEnabled(category);
}

void logging::setDebugging(bool enabled) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.setDebugging(enabled);
}

bool logging::isDebugSession() {
    QMutexLocker lock(&s_logger_mutex);
    return s_logger.isDebugSession();
}

void logging::setDeduplicateInfo(bool enabled) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.setDeduplicateInfo(enabled);
}

bool logging::deduplicateInfo() {
    QMutexLocker lock(&s_logger_mutex);
    return s_logger.deduplicateInfo();
}

void logging::debug(const QString &message, LogCategory category) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.debug(message, category);
}

void logging::info(const QString &message, LogCategory category) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.info(message, category);
}

void logging::warn(const QString &message, LogCategory category) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.warn(message, category);
}

void logging::error(const QString &message, LogCategory category) {
    QMutexLocker lock(&s_logger_mutex);
    s_logger.error(message, category);
}
