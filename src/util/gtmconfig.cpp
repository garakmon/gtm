#include "gtmconfig.h"

#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include "orderedjson.h"

QString GtmConfig::fileName() {
    return "gtm.config";
}

QString GtmConfig::defaultPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!base.isEmpty()) {
        QDir().mkpath(base);
        return QDir(base).filePath(fileName());
    }
    return fileName();
}

GtmConfig GtmConfig::loadFromFile(const QString &path, bool *ok) {
    GtmConfig cfg;
    if (ok) *ok = false;

    QFile file(path);
    if (!file.exists()) {
        return cfg;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return cfg;
    }

    const QByteArray data = file.readAll();
    file.close();

    QString parse_err;
    orderedjson::Json json = orderedjson::Json::parse(QString::fromUtf8(data), &parse_err);
    if (!parse_err.isEmpty() || !json.is_object()) {
        return cfg;
    }

    const orderedjson::Json::object &obj = json.object_items();
    auto it_recent = obj.find("most_recent_project");
    if (it_recent != obj.end() && it_recent->second.is_string()) {
        cfg.most_recent_project = it_recent->second.string_value();
    }
    auto it_recent_song = obj.find("recent_song");
    if (it_recent_song != obj.end() && it_recent_song->second.is_string()) {
        cfg.recent_song = it_recent_song->second.string_value();
    }
    auto it_master = obj.find("master_volume");
    if (it_master != obj.end() && it_master->second.is_number()) {
        cfg.master_volume = static_cast<int>(it_master->second.number_value());
    }
    auto it_palette = obj.find("palette");
    if (it_palette != obj.end() && it_palette->second.is_string()) {
        cfg.palette = it_palette->second.string_value();
        if (cfg.palette.isEmpty()) cfg.palette = "default";
    }
    auto it_theme = obj.find("theme");
    if (it_theme != obj.end() && it_theme->second.is_string()) {
        cfg.theme = it_theme->second.string_value();
        if (cfg.theme.isEmpty()) cfg.theme = "default";
    }
    auto it_geom = obj.find("window_geometry");
    if (it_geom != obj.end() && it_geom->second.is_string()) {
        cfg.window_geometry = it_geom->second.string_value();
    }

    if (ok) *ok = true;
    return cfg;
}

bool GtmConfig::saveToFile(const QString &path) const {
    orderedjson::Json::object obj;
    obj["most_recent_project"] = most_recent_project;
    obj["recent_song"] = recent_song;
    obj["master_volume"] = master_volume;
    obj["palette"] = palette.isEmpty() ? "default" : palette;
    obj["theme"] = theme.isEmpty() ? "default" : theme;
    obj["window_geometry"] = window_geometry;
    orderedjson::Json json(obj);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    orderedjson::JsonDoc doc(&json);
    doc.dump(&file);
    file.close();
    return true;
}
