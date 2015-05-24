#ifndef DATABASE_H
#define DATABASE_H

#include <QtSql>
#include <QString>
#include <QObject>
#include <QSqlDatabase>

class Database : public QObject
{
    Q_OBJECT

signals:
    void information( QString qsInfo ) ;
    void debug( QString qsFatalError ) ;
    void warning( QString qsWarning ) ;
    void fatal( QString qsFatalError ) ;

public:
    Database(QString host, QString name, QString user, QString pass);
    ~Database();
    /**
     * @brief connexion
     * @details Se connecter à une base PostgreSQL locale
     * @return
     */
    bool connexion();

    //SUPPRESSION DE TABLE
    bool dropTable(QString tableName, QString schemaName);

    //SUPPRESSION DE COLONNE DANS UNE TABLE
    bool dropColumn(QString tableName, QString columnName, QString schemaName);

    //EXISTENCE DE TABLE
    bool tableExists(QString tableName, QString schemaName);

    //EXISTENCE DE SCHEMA
    bool schemaExists(QString schemaName);

    //EXISTENCE D'UNE COLONNE DANS UNE TABLE
    bool columnExists(QString tableName, QString columnName, QString schemaName);

    //ajout d'un attribut combiné par division
    bool add_att_div(QString table, QString new_att, QString att_1, QString att_2, QString schemaName);

    //ajout d'un attribut combiné par multiplication
    bool add_att_prod(QString table, QString new_att, QString att_1, QString att_2, QString schemaName);

    //ajout d'un attribut combiné par soustraction
    bool add_att_dif(QString table, QString new_att, QString att_1, QString att_2, QString schemaName);

    //ajout d'un attribut combiné par soustraction
    bool add_att_difABS(QString table, QString new_att, QString att_1, QString att_2, QString schemaName);

    //ajout d'un attribut combiné par addition
    bool add_att_add(QString table, QString new_att, QString att_1, QString att_2, QString schemaName);

    //ajout d'un attribut de classification
    bool add_att_cl(QString table, QString new_att, QString att_1, int nb_classes, bool ascendant, QString schemaName);

private:
    QSqlDatabase m_db;
    QString m_host;
    QString m_name;
    QString m_user;
    QString m_pass;
};

#endif // DATABASE_H
