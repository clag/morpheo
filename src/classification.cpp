#include "MainWindow.h"
#include "Database.h"
#include "Graphe.h"
#include "Voies.h"
//#include "Sommet.h"
#include "Logger.h"

#include <QApplication>
#include <QtSql>
#include <QTableView>
#include <QTextCodec>

#include <unistd.h>
#include <iostream>
using namespace std;


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //MainWindow w;
    //w.show();

    QString host = "localhost";
    QString name = "avignon_ech1";
    QString user = "postgres";
    QString pass = "post123";

    Database workDatabase(host, name, user, pass);

    QDateTime start = QDateTime::currentDateTime();
    Logger::INFO(QString("Started : %1").arg(start.toString()));

    if (workDatabase.connexion()) {
        //------------------------------- Connexion établie
        Logger::INFO("Connexion OK");

    } else {
        Logger::ERREUR("Erreur Connexion");
        sleep(1);
        exit(EXIT_FAILURE);
    }

    QString table = "test1";
    QString new_att1 = "cl_bet";
    QString att_1 = "betweenness";
    int nb_classes = 10;
    bool ascendant = true;

    if (! Database::add_att_cl(table, new_att1, att_1, nb_classes, ascendant)) exit(EXIT_FAILURE);

    /*QString diff1 = "DIFF_useBet";
    QString use = "cl_use";
    QString bet = "cl_bet";

    QString diff2 = "DIFF_useMltBet";
    QString usemlt = "cl_usemlt";

    QString type = "integer";

    if (! Database::add_att_difABS(table, diff1, type, use, bet)) return false;
    if (! Database::add_att_difABS(table, diff2, type, usemlt, bet)) return false;*/

    /*QString table = "arcs_1_cent_line";
    QString new_att1 = "rea_cl";
    QString att_1 = "reach";
    QString new_att2 = "bet_cl";
    QString att_2 = "betweennes";
    QString new_att3 = "str_cl";
    QString att_3 = "straightne";
    QString new_att4 = "clo_cl";
    QString att_4 = "closeness";
    QString new_att5 = "gra_cl";
    QString att_5 = "gravity";
    int nb_classes = 10;

    if (! Database::add_att_cl(table, new_att1, att_1, nb_classes, true)) exit(EXIT_FAILURE);
    if (! Database::add_att_cl(table, new_att2, att_2, nb_classes, true)) exit(EXIT_FAILURE);
    if (! Database::add_att_cl(table, new_att3, att_3, nb_classes, true)) exit(EXIT_FAILURE);
    if (! Database::add_att_cl(table, new_att4, att_4, nb_classes, true)) exit(EXIT_FAILURE);
    if (! Database::add_att_cl(table, new_att5, att_5, nb_classes, true)) exit(EXIT_FAILURE);*/

    QDateTime end = QDateTime::currentDateTime();
    Logger::INFO(QString("End : %1").arg(end.toString()));
    Logger::INFO(QString("Temps total d'execution' : %1 minutes").arg(start.secsTo(end) / 60.));


    sleep(1);
    exit(EXIT_SUCCESS);
    return a.exec();
}

