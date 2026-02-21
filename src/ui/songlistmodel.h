#pragma once
#ifndef SONGLISTMODEL_H
#define SONGLISTMODEL_H

#include <QAbstractListModel>

#include <memory>



class Project;

class SongListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit SongListModel(Project *project, QObject *parent = nullptr)
        : QAbstractListModel(parent), m_project(project) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void refreshAll();

private:
    Project *m_project; // make sure the model is deleted if ever other projects are allowed to be opened
};

#endif // SONGLISTMODEL_H
