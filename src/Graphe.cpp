#include "Graphe.h"

#include <QtSql>
#include <QString>

#include <iostream>
using namespace std;

#include <stdlib.h>
#include <math.h>
#include <sstream>

#include <vector>
#define PI 3.1415926535897932384626433832795

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE SXYZ
//
//***************************************************************************************************************************************************
bool Graphe::build_SXYZ(){


    //PRESENCE DE LA TABLE SXYZ ? SI DEJA EXISTANTE, rien à faire, sauf récupérer le nombre de sommets

    if (! pDatabase->tableExists("SXYZ")) {
        pLogger->INFO("-------------------------- build_SXYZ START --------------------------");

        // CREATION DES TABLES SXYZ

        QSqlQueryModel createSXYZ;
        createSXYZ.setQuery("CREATE TABLE SXYZ ( IDS SERIAL NOT NULL PRIMARY KEY, SX double precision, SY double precision, SZ double precision, GEOM geometry, DEG integer);");

        if (createSXYZ.lastError().isValid()) {
            pLogger->ERREUR(QString("createSXYZ : %1").arg(createSXYZ.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        pLogger->INFO("--------------------------- build_SXYZ END ---------------------------");
    } else {
        pLogger->INFO("------------------------- SXYZ already exists ------------------------");
        // On récupère le nombre de sommets
        QSqlQueryModel nbSommets;
        nbSommets.setQuery("SELECT COUNT(*) AS nb FROM SXYZ; ");

        if (nbSommets.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de récupérer le nombre de sommets : %1").arg(nbSommets.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        m_nbSommets = nbSommets.record(0).value("nb").toInt();
    }

    return true;

}// END build_SXYZ


//***************************************************************************************************************************************************
//AJOUT D'UN SOMMET DANS LA TABLE SXYZ
//
//***************************************************************************************************************************************************

/** \brief Ajoute le point fourni comme impasse dans la table SXYZ
 * \return Retourne l'ID du sommet insérer, -1 en cas d'erreur
 */
int Graphe::ajouterSommet(QVariant point){

    m_nbSommets++;
    double ids = m_nbSommets;

    QSqlQuery addSXYZ;
    addSXYZ.prepare("INSERT INTO SXYZ (SX, SY, SZ, GEOM, DEG, IDS) "
                    "VALUES (ST_X(:GEOM), ST_Y(:GEOM), ST_Z(:GEOM), :GEOM, 1, :IDS);");
    addSXYZ.bindValue(":IDS",QVariant(ids));
    addSXYZ.bindValue(":GEOM",point);

    if (! addSXYZ.exec()) {
        pLogger->ERREUR(QString("Impossible d'inserer les coordonnées du sommet %1 dans SXYZ").arg(ids));
        pLogger->ERREUR(addSXYZ.lastError().text());
        pLogger->ERREUR(QString("Query : %1").arg(addSXYZ.lastQuery()));
        pLogger->ERREUR(QString("Point : %1").arg(point.toString()));
        return -1;
    }

    return ids;

}// END ajout_sxyz


//***************************************************************************************************************************************************
//RECHERCHE DE L'IDENTIFIANT D'UN SOMMET A PARTIR DE SES COORDONNEES / AJOUTS DANS LA TABLE DES IMPASSES SI NON TROUVE
//
//***************************************************************************************************************************************************

/** \brief Retourne l'identifiant du sommet correpondant au point fourni, -1 en cas d'erreur.
 * \abstract Si le sommet n'existe pas en base, on l'ajoute en tant que impasse
 */
int Graphe::find_ids(QVariant point){

    QSqlQuery req_sxyz;
    req_sxyz.prepare("SELECT IDS, DEG FROM SXYZ WHERE ST_Distance(GEOM, :POINT) < :E");
    req_sxyz.bindValue(":POINT", point);
    req_sxyz.bindValue(":E", m_toleranceSommets);

    if (! req_sxyz.exec()) {
        pLogger->ERREUR(QString("req_sxyz : %1").arg(req_sxyz.lastError().text()));
        return -1;
    }

    QSqlQueryModel req_sxyz_model;
    req_sxyz_model.setQuery(req_sxyz);

    if(req_sxyz_model.rowCount() > 1){
        // Plusieurs sommets possibles (triés par distance croissante : on informe et on renvoie l'ID du sommet le plus proche
        pLogger->ATTENTION(QString("req_sxyz : ambiguite - nombre de reponses = %1").arg(req_sxyz_model.rowCount()));

        QSqlQuery updateDeg;
        updateDeg.prepare("UPDATE SXYZ SET DEG=:DEG WHERE IDS=:IDS");
        updateDeg.bindValue(":IDS", req_sxyz_model.record(0).value("IDS").toInt());
        updateDeg.bindValue(":DEG", req_sxyz_model.record(0).value("DEG").toInt() + 1);

        if (! updateDeg.exec()) {
            pLogger->ERREUR(QString("updateDeg : %1").arg(req_sxyz.lastError().text()));
            return -1;
        }

        return req_sxyz_model.record(0).value("IDS").toInt();
    } else if (req_sxyz_model.rowCount() == 1 ) {
        QSqlQuery updateDeg;
        updateDeg.prepare("UPDATE SXYZ SET DEG=:DEG WHERE IDS=:IDS");
        updateDeg.bindValue(":IDS", req_sxyz_model.record(0).value("IDS").toInt());
        updateDeg.bindValue(":DEG", req_sxyz_model.record(0).value("DEG").toInt() + 1);

        if (! updateDeg.exec()) {
            pLogger->ERREUR(QString("updateDeg : %1").arg(req_sxyz.lastError().text()));
            return -1;
        }

        return req_sxyz_model.record(0).value("IDS").toInt();
    } else {
        int id = ajouterSommet(point);
        if (id < 0) {
            pLogger->ERREUR(QString("Impossible d'ajouter le point suivant comme impasse : %1").arg(point.toString()));
            return -1;
        }
        return id;
    }

}// END find_ids

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE SIF
//
//***************************************************************************************************************************************************

bool Graphe::build_SIF(QSqlQueryModel* arcs_bruts){


    //PRESENCE DE LA TABLE SIF ? SI DEJA EXISTANTE, rien à faire

    if (! pDatabase->tableExists("SIF")) {

        pLogger->INFO("--------------------------- build_SIF START --------------------------");

        //CREATION DE LA TABLE SIF D'ACCUEIL DEFINITIVE

        QSqlQueryModel createTableSIF;
        createTableSIF.setQuery("CREATE TABLE SIF ( IDA SERIAL NOT NULL PRIMARY KEY, SI bigint, SF bigint, AZIMUTH_I double precision, AZIMUTH_F double precision, "
                                "FOREIGN KEY (SI) REFERENCES SXYZ(IDS), "
                                "FOREIGN KEY (SF) REFERENCES SXYZ(IDS) );");

        if (createTableSIF.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_sif : %1").arg(createTableSIF.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        //EXTRACTION DES DONNEES
        for(int i=0; i<m_nbArcs; i++){

            int ida = arcs_bruts->record(i).value("IDA").toInt();
            QVariant point_si = arcs_bruts->record(i).value("POINT_SI");
            QVariant point_sf = arcs_bruts->record(i).value("POINT_SF");

            //IDENTIFICATION DES SOMMETS SI ET SF DANS LA TABLE DES SOMMETS
            int id_si = this->find_ids(point_si);
            int id_sf = this->find_ids(point_sf);
            if (id_si < 0 || id_sf < 0) {
                pLogger->ERREUR("Erreur dans l'identification des sommets initial et final");
                return false;
            }

            //SAUVEGARDE EN BASE
            QSqlQuery addInSIF;
            addInSIF.prepare("INSERT INTO SIF(IDA, SI, SF, AZIMUTH_I, AZIMUTH_F) VALUES (:IDA, :SI, :SF, :AI, :AF);");
            addInSIF.bindValue(":IDA",QVariant(ida));
            addInSIF.bindValue(":SI",QVariant(id_si));
            addInSIF.bindValue(":SF",QVariant(id_sf));
            addInSIF.bindValue(":AI",arcs_bruts->record(i).value("AZIMUTH_I").toDouble() * 180.0 / PI);
            addInSIF.bindValue(":AF",arcs_bruts->record(i).value("AZIMUTH_F").toDouble() * 180.0 / PI);

            if (! addInSIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'identifiant des sommets %1 et %2 dans SIF. Arc : %3 / record %4").arg(id_si).arg(id_sf).arg(ida).arg(i));
                pLogger->ERREUR(addInSIF.lastError().text());
                return false;
            }

        }//endfor

        pLogger->INFO("--------------------------- build_SIF END ----------------------------");
    } else {
        pLogger->INFO("-------------------------- SIF already exists ------------------------");
    }



    QSqlQueryModel nbImpasses;
    nbImpasses.setQuery("SELECT COUNT(*) AS nb FROM SXYZ WHERE DEG=1; ");

    if (nbImpasses.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre d'impasses : %1").arg(nbImpasses.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbImpasses = nbImpasses.record(0).value("nb").toInt();



    QSqlQueryModel nbIntersections;
    nbIntersections.setQuery("SELECT COUNT(*) AS nb FROM SXYZ WHERE DEG>1; ");

    if (nbIntersections.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre d'intersections : %1").arg(nbIntersections.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbIntersections = nbIntersections.record(0).value("nb").toInt();



    QSqlQueryModel nbSommetsNul;
    nbSommetsNul.setQuery("SELECT COUNT(*) AS nb FROM SXYZ WHERE DEG=0; ");

    if (nbSommetsNul.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre de sommets isolés : %1").arg(nbSommetsNul.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    if (nbSommetsNul.record(0).value("nb").toInt() != 0) {
        pLogger->ATTENTION("Il ne devrait pas y avoir de sommets isolés (degré nul)");
        return false;
    }



    pLogger->INFO(QString("Nombre total de sommets : %1, dont %2 intersections et %3 impasses").arg(m_nbSommets).arg(m_nbIntersections).arg(m_nbImpasses));

    if (m_nbSommets != m_nbImpasses + m_nbIntersections) {
        pLogger->ERREUR("Nombre de sommets, impasses et intersections incohérent : ça cache un mal-être");
        return false;
    }

    return true;
}

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE ANGLES
//
//***************************************************************************************************************************************************

bool Graphe::build_ANGLES(){


    if (! pDatabase->tableExists("ANGLES")) {

        pLogger->INFO("-------------------------- build_ANGLES START ------------------------");

        QSqlQueryModel createTableANGLES;
        createTableANGLES.setQuery("CREATE TABLE ANGLES (ID SERIAL NOT NULL PRIMARY KEY, IDS bigint, IDA1 bigint, IDA2 bigint, ANGLE double precision, USED boolean, "
                                "FOREIGN KEY (IDS) REFERENCES SXYZ(IDS), FOREIGN KEY (IDA1) REFERENCES SIF(IDA), FOREIGN KEY (IDA2) REFERENCES SIF(IDA) );");

        if (createTableANGLES.lastError().isValid()) {
            pLogger->ERREUR(QString("createTableANGLES : %1").arg(createTableANGLES.lastError().text()));
            return false;
        }

        QSqlQueryModel* modelArcs = new QSqlQueryModel();
        QSqlQuery queryArcs;
        queryArcs.prepare("SELECT IDA AS IDA, SI AS SI, SF AS SF, AZIMUTH_I AS AI, AZIMUTH_F AS AF FROM SIF WHERE SI = :IDSI OR SF = :IDSF;");


        QSqlQuery addAngle;
        addAngle.prepare("INSERT INTO ANGLES(IDS, IDA1, IDA2, ANGLE, USED) VALUES (:IDS, :IDA1, :IDA2, :ANGLE, FALSE);");

        for (int s = 1; s < m_nbSommets; s++) {
            queryArcs.bindValue(":IDSI",s);
            queryArcs.bindValue(":IDSF",s);

            if (! queryArcs.exec()) {
                pLogger->ERREUR(QString("Récupération arcs dans build_ANGLES pour le sommet %1 : %2").arg(s).arg(queryArcs.lastError().text()));
                return false;
            }

            modelArcs->setQuery(queryArcs);

            for (int a1 = 0; a1 < modelArcs->rowCount(); a1++) {
                int ida1 = modelArcs->record(a1).value("IDA").toInt();
                int si1 = modelArcs->record(a1).value("SI").toInt();
                int sf1 = modelArcs->record(a1).value("SF").toInt();
                int azi1 = modelArcs->record(a1).value("AI").toDouble();
                int azf1 = modelArcs->record(a1).value("AF").toDouble();

                // On traite le cas particulier de l'arc boucle (si = sf)
                if (si1 == sf1 && si1 == s) {
                    double angle = fabs(azi1 - azf1);
                    if (angle > 180.) angle = 360.0 - angle;
                    angle = fabs(angle - 180.);

                    addAngle.bindValue(":IDS",s);
                    addAngle.bindValue(":IDA1",ida1);
                    addAngle.bindValue(":IDA2",ida1);
                    addAngle.bindValue(":ANGLE",angle);

                    if (! addAngle.exec()) {
                        pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et lui-même (arc boucle) : %2").arg(ida1).arg(addAngle.lastError().text()));
                        return false;
                    }
                }


                for (int a2 = a1 + 1; a2 < modelArcs->rowCount(); a2++) {

                    int ida2 = modelArcs->record(a2).value("IDA").toInt();
                    int si2 = modelArcs->record(a2).value("SI").toInt();
                    int sf2 = modelArcs->record(a2).value("SF").toInt();
                    int azi2 = modelArcs->record(a2).value("AI").toDouble();
                    int azf2 = modelArcs->record(a2).value("AF").toDouble();

                    if (si1 == si2 && si1 == s) {
                        double angle = fabs(azi1 - azi2);
                        if (angle > 180.) angle = 360.0 - angle;
                        angle = fabs(angle - 180.);

                        addAngle.bindValue(":IDS",s);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (sf1 == si2 && sf1 == s) {
                        double angle = fabs(azf1 - azi2);
                        if (angle > 180.) angle = 360.0 - angle;
                        angle = fabs(angle - 180.);

                        addAngle.bindValue(":IDS",s);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (si1 == sf2 && si1 == s) {
                        double angle = fabs(azi1 - azf2);
                        if (angle > 180.) angle = 360.0 - angle;
                        angle = fabs(angle - 180.);

                        addAngle.bindValue(":IDS",s);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (sf1 == sf2 && sf1 == s) {
                        double angle = fabs(azf1 - azf2);
                        if (angle > 180.) angle = 360.0 - angle;
                        angle = fabs(angle - 180.);

                        addAngle.bindValue(":IDS",s);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }
                }
            }

        }

        delete modelArcs;

        pLogger->INFO("--------------------------- build_ANGLES END -------------------------");
    } else {
        pLogger->INFO("------------------------ ANGLES already exists -----------------------");
    }

    return true;
}

//***************************************************************************************************************************************************
//CONSTRUCTION DES TABLES AXYZ, PAXYZ
//
//***************************************************************************************************************************************************

bool Graphe::build_P_AXYZ(QSqlQueryModel* arcs_bruts){


    if (! pDatabase->tableExists("AXYZ") || ! pDatabase->tableExists("PAXYZ")) {

        pLogger->INFO("------------------------ build_P_AXYZ START --------------------------");

        //CREATION DES TABLES AXYZ ET PAXYZ D'ACCUEIL DEFINITIVES

        QSqlQueryModel createtable_axyz;
        createtable_axyz.setQuery("CREATE TABLE AXYZ (IDA SERIAL NOT NULL PRIMARY KEY, LI bigint, LF bigint, GEOM geometry, FOREIGN KEY (IDA) REFERENCES SIF(IDA) );");

        if (createtable_axyz.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_axyz : %1").arg(createtable_axyz.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        QSqlQueryModel createtable_paxyz;
        createtable_paxyz.setQuery("CREATE TABLE PAXYZ (IDPA SERIAL NOT NULL PRIMARY KEY, PAX double precision, PAY double precision, PAZ double precision );");

        if (createtable_paxyz.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_paxyz : %1").arg(createtable_paxyz.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        //EXTRACTION DES DONNEES

        //initialisation du pointeur de ligne dans paxyz
        int li_paxyz = 0;
        int lf_paxyz = 0;


        QSqlQuery addInAXYZ;
        addInAXYZ.prepare("INSERT INTO AXYZ (IDA, LI, LF, GEOM) VALUES (:IDA, :LI, :LF, :GEOM);");

        for(int i=0; i<m_nbArcs; i++){

            //identifiant de l'arc
            int ida = arcs_bruts->record(i).value("IDA").toInt();

            //géométrie de l'arc
            QVariant geomArcs = arcs_bruts->record(i).value("GEOM");

            //nombre de points annexes de l'arc
            int num_points = arcs_bruts->record(i).value("NUMPOINTS").toInt();

            //Si pas de point annexe sur l'arc traité
            if (num_points == 2) {
                addInAXYZ.bindValue(":IDA",ida);
                addInAXYZ.bindValue(":LI",0);
                addInAXYZ.bindValue(":LF",0);
                addInAXYZ.bindValue(":GEOM",geomArcs);

                if (! addInAXYZ.exec()) {
                    pLogger->ERREUR(QString("Impossible d'inserer l'arc %1 dans AXYZ. / record : %2").arg(ida).arg(i));
                    pLogger->ERREUR(addInAXYZ.lastError().text());
                    return false;
                }

                continue;
            }

            //ligne finale dans PAXYZ = ligne initiale dans PAXYZ + nb de points -1
            lf_paxyz = li_paxyz + (num_points-2) -1;

            //SAUVEGARDE EN BASE DES VALEURS DE AXYZ
            addInAXYZ.bindValue(":IDA",ida);
            addInAXYZ.bindValue(":LI",li_paxyz);
            addInAXYZ.bindValue(":LF",lf_paxyz);
            addInAXYZ.bindValue(":GEOM",geomArcs);

            if (! addInAXYZ.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'arc %1 dans AXYZ. / record : %2").arg(ida).arg(i));
                pLogger->ERREUR(addInAXYZ.lastError().text());
                return false;
            }

            for (int j=2; j<num_points; j++){

                //on sélectionne le j(ième) point du i(ième) arc.
                QSqlQuery addInPAXYZ;
                addInPAXYZ.prepare("INSERT INTO PAXYZ (PAX, PAY, PAZ) VALUES "
                                  "(ST_X(ST_PointN(:GEOM1,:N1)), "
                                  "ST_Y(ST_PointN(:GEOM2,:N2)), "
                                  "ST_Z(ST_PointN(:GEOM3,:N3)));");

                addInPAXYZ.bindValue(":GEOM1",geomArcs);
                addInPAXYZ.bindValue(":GEOM2",geomArcs);
                addInPAXYZ.bindValue(":GEOM3",geomArcs);
                addInPAXYZ.bindValue(":N1",j);
                addInPAXYZ.bindValue(":N2",j);
                addInPAXYZ.bindValue(":N3",j);

                if (! addInPAXYZ.exec()) {
                    pLogger->ERREUR(QString("Impossible d'inserer le point annexe %1 dans PAXYZ. / record : %2").arg(li_paxyz+j).arg(j));
                    pLogger->ERREUR(addInPAXYZ.lastError().text());
                    return false;
                }

            }//endforj

            li_paxyz = lf_paxyz +1;

        }//endfori

        pLogger->INFO("------------------------- build_P_AXYZ END ---------------------------");

    } else {
        pLogger->INFO("--------------------- AXYZ et PAXYZ already exist --------------------");
    }

    QSqlQueryModel nbPointsAnnexes;
    nbPointsAnnexes.setQuery("SELECT COUNT(*) AS nb FROM PAXYZ; ");

    if (nbPointsAnnexes.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre de points annexes : %1").arg(nbPointsAnnexes.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbPointsAnnexes = nbPointsAnnexes.record(0).value("nb").toInt();

    pLogger->INFO(QString("Nombre de points annexes : %1").arg(m_nbPointsAnnexes));

    return true;

}// END build_arcs

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_SOMARCS
//
//***************************************************************************************************************************************************

bool Graphe::build_SomArcs(){

    pLogger->INFO("------------------------ build_SomArcs START -------------------------");

    //REQUETE SUR SIF
    QSqlQueryModel *tableSIF = new QSqlQueryModel();
    tableSIF->setQuery("SELECT ida AS IDA, si AS SI, sf AS SF FROM SIF;");

    if (tableSIF->lastError().isValid()) {
        pLogger->ERREUR(QString("tableSIF : %1").arg(tableSIF->lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    // Dimensionnement du vecteur
    m_SomArcs.resize(m_nbSommets + 1);

    for(int j=0; j<m_nbArcs; j++){

        int ida = tableSIF->record(j).value("IDA").toInt();
        int si = tableSIF->record(j).value("SI").toInt();
        int sf = tableSIF->record(j).value("SF").toInt();

        m_SomArcs[si].push_back(ida);

        // Dans le cas d'un arc "boucle" (sommet final = sommet initial), on ne doit pas ajouter l'arc au sommet une deuxième fois
        //if (si != sf) {
            m_SomArcs[sf].push_back(ida);
        //}

    }//end for j

    delete tableSIF;


    QSqlQuery addDegresInSXYZ;
    addDegresInSXYZ.prepare("UPDATE SXYZ SET DEG = :DEG WHERE ids = :IDS ;");
    for(int s=1; s <= m_nbSommets; s++){

        int degres = m_SomArcs[s].size();

        //INSERTION EN BASE
        addDegresInSXYZ.bindValue(":DEG",degres);
        addDegresInSXYZ.bindValue(":IDS",s);

        if (! addDegresInSXYZ.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer le degres du sommet ids %1").arg(s));
            pLogger->ERREUR(addDegresInSXYZ.lastError().text());
            return false;
        }

    }//end for s

    pLogger->INFO("------------------------- build_SomArcs END --------------------------");

    return true;

}// END build_SomArcs

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_ARC_ARCS
//
//***************************************************************************************************************************************************

bool Graphe::build_ArcArcs(){

    pLogger->INFO("------------------------ build_ArcArcs START -------------------------");

    m_ArcArcs.resize(m_nbArcs + 1);

    for(int ids=1; ids <= m_nbSommets; ids++) {

        //ARCS TOUCHANT SOMMET INITIAL
        for(int i = 0; i < m_SomArcs[ids].size(); i++){

            int ida1 = m_SomArcs[ids][i];

            for(int j = i+1; j < m_SomArcs[ids].size(); j++){
                int ida2 = m_SomArcs[ids][j];

                // On Vérifie bien que les arcs n'ont pas déjà été identifiés comme voisin
                if (m_ArcArcs[ida1].indexOf(ida2) < 0) {
                    m_ArcArcs[ida1].push_back(ida2);
                    m_ArcArcs[ida2].push_back(ida1);
                }
            }
        }
    }

    pLogger->INFO("------------------------- build_ArcArcs END --------------------------");

    return true;

}// END build_ArcArcs

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE INFO EN BDD
//
//***************************************************************************************************************************************************

bool Graphe::insertINFO(){

    pLogger->INFO("-------------------------- insertINFO START --------------------------");

    if (! pDatabase->tableExists("INFO")) {
        QSqlQueryModel createTableINFO;
        createTableINFO.setQuery("CREATE TABLE INFO ( "
                                  "ID SERIAL NOT NULL PRIMARY KEY, "
                                  "methode integer, "
                                  "seuil_angle integer, "
                                  "LTOT float, "
                                  "NS integer, "
                                  "NA integer, "
                                  "NPA integer, "
                                  "N_INTER integer, "
                                  "N_IMP integer, "
                                  "N_COUPLES integer, "
                                  "N_CELIBATAIRES integer, "
                                  "NV integer, "

                                  "AVG_LVOIE float, "
                                  "STD_LVOIE float, "

                                  "AVG_ANG float, "
                                  "STD_ANG float, "

                                  "AVG_NBA float, "
                                  "STD_NBA float, "

                                  "AVG_NBS float, "
                                  "STD_NBS float, "

                                  "AVG_NBC float, "
                                  "STD_NBC float, "

                                  "AVG_O float, "
                                  "STD_O float, "

                                  "AVG_S float, "
                                  "STD_S float, "


                                  "AVG_LOG_LVOIE float, "
                                  "STD_LOG_LVOIE float, "

                                  "AVG_LOG_NBA float, "
                                  "STD_LOG_NBA float, "

                                  "AVG_LOG_NBS float, "
                                  "STD_LOG_NBS float, "

                                  "AVG_LOG_NBC float, "
                                  "STD_LOG_NBC float, "

                                  "AVG_LOG_O float, "
                                  "STD_LOG_O float, "

                                  "AVG_LOG_S float, "
                                  "STD_LOG_S float, "

                                  "ACTIVE boolean "
                                  ");");

        if (createTableINFO.lastError().isValid()) {
            pLogger->ERREUR("Impossible de créer la table INFO");
            pLogger->ERREUR(createTableINFO.lastError().text());
            return false;
        }

        QSqlQuery addInfos;
        addInfos.prepare("INSERT INTO INFO (NS, NA, NPA, N_INTER, N_IMP, ACTIVE) VALUES (:NS, :NA, :NPA, :N_INTER, :N_IMP, FALSE);");
        addInfos.bindValue(":NS",QVariant(m_nbSommets));
        addInfos.bindValue(":NA",QVariant(m_nbArcs));
        addInfos.bindValue(":NPA",QVariant(m_nbPointsAnnexes));
        addInfos.bindValue(":N_INTER",QVariant(m_nbIntersections));
        addInfos.bindValue(":N_IMP",QVariant(m_nbImpasses));

        if (! addInfos.exec()) {
            pLogger->ERREUR("Impossible d'inserer les infos dans la table");
            pLogger->ERREUR(addInfos.lastError().text());
            return false;
        }
    }

    pLogger->INFO("-------------------------- insertINFO END ----------------------------");

    return true;

}//END create_info

//***************************************************************************************************************************************************

//***************************************************************************************************************************************************
//CONSTRUCTION DES TABLES DU GRAPHE
//
//***************************************************************************************************************************************************

bool Graphe::do_Graphe(QString brutArcsTableName){

    //construction des tables en BDD
    if (! build_SXYZ()) {
        if (! pDatabase->dropTable("SXYZ")) {
            pLogger->ERREUR("build_SXYZ en erreur, ROLLBACK (drop SXYZ) échoué");
        } else {
            pLogger->INFO("build_SXYZ en erreur, ROLLBACK (drop SXYZ) réussi");
        }
        return false;
    }

    // Récupération des arcs bruts
    QSqlQueryModel *arcs_bruts = new QSqlQueryModel();

    arcs_bruts->setQuery(QString("SELECT "
                         "gid AS IDA, "
                         "geom AS GEOM,"
                         "ST_NumPoints(geom) AS NUMPOINTS,"

                         "ST_StartPoint(geom) AS POINT_SI, "
                         "ST_EndPoint(geom) AS POINT_SF, "

                         "ST_azimuth(ST_StartPoint(geom), ST_PointN(geom,2)) AS AZIMUTH_I, "
                         "ST_azimuth(ST_EndPoint(geom), ST_PointN(geom,ST_NumPoints(geom)-1)) AS AZIMUTH_F "

                         "FROM %1").arg(brutArcsTableName));

    if (arcs_bruts->lastError().isValid()) {
        pLogger->ERREUR(QString("Récupération arcs_bruts : %1").arg(arcs_bruts->lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel


    //NOMBRE D'ARCS    (nombres de lignes du résultat de la requête)
    m_nbArcs = arcs_bruts->rowCount();

    pLogger->INFO(QString("Nombre d'arcs : %1").arg(m_nbArcs));

    if (! build_SIF(arcs_bruts)) {
        if (! pDatabase->dropTable("SIF")) {
            pLogger->ERREUR("build_SIF en erreur, ROLLBACK (drop SIF) échoué");
        } else {
            pLogger->INFO("build_SIF en erreur, ROLLBACK (drop SIF) réussi");
        }
        if (! pDatabase->dropTable("SXYZ")) {
            pLogger->ERREUR("build_SIF en erreur, ROLLBACK (drop SXYZ) échoué");
        } else {
            pLogger->INFO("build_SIF en erreur, ROLLBACK (drop SXYZ) réussi");
        }
        return false;
    }

    if (! build_P_AXYZ(arcs_bruts)) {
        if (! pDatabase->dropTable("AXYZ")) {
            pLogger->ERREUR("build_P_AXYZ en erreur, ROLLBACK (drop AXYZ) échoué");
        } else {
            pLogger->INFO("build_P_AXYZ en erreur, ROLLBACK (drop AXYZ) réussi");
        }

        if (! pDatabase->dropTable("PAXYZ")) {
            pLogger->ERREUR("build_P_AXYZ en erreur, ROLLBACK (drop PAXYZ) échoué");
        } else {
            pLogger->INFO("build_P_AXYZ en erreur, ROLLBACK (drop PAXYZ) réussi");
        }
        return false;
    }

    delete arcs_bruts;

    //construction des matrices attributs membres des arcs
    if (! build_SomArcs()) return false;
    if (! build_ArcArcs()) return false;

    if (! build_ANGLES()) {
        if (! pDatabase->dropTable("ANGLES")) {
            pLogger->ERREUR("build_ANGLES en erreur, ROLLBACK (drop ANGLES) échoué");
        } else {
            pLogger->INFO("build_ANGLES en erreur, ROLLBACK (drop ANGLES) réussi");
        }
        return false;
    }

    //table d'informations du graphe
    if (! insertINFO()) return false;

    return true;

}//end build_Graphe


bool Graphe::getSommetsOfArcs(int ida, int* si, int* sf) {
    QSqlQuery queryArc;
    queryArc.prepare("SELECT SI, SF FROM SIF WHERE IDA = :IDA;");

    queryArc.bindValue(":IDA",ida);

    if (! queryArc.exec()) {
        pLogger->ERREUR(QString("Récupération des sommets de l'arc %1").arg(ida));
        return false;
    }

    queryArc.next();

    *si = queryArc.record().value(0).toInt();
    *sf = queryArc.record().value(1).toInt();

    return true;
}

/*bool Graphe::build_ArcsAzFromSommet(int ids, int buffer, float* tabAzArcs) {

    //, int* ida, float* azi, float* azf, float* dist

    //extraction de la géométrie du sommet ------------------------------------------------------------------------------

    QSqlQueryModel GeomFromSXYZ;

    QSqlQuery querySxyz;
    querySxyz.prepare("SELECT GEOM FROM SXYZ WHERE IDS = :IDS ;");
    querySxyz.bindValue(":IDS",ids);

    GeomFromSXYZ.setQuery(querySxyz);

    if (GeomFromSXYZ.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de recuperer des infos (SXYZ) : %1").arg(GeomFromSXYZ.lastError().text()));
        return false;
    }//end if : test requete QSqlQueryModel

    if(GeomFromSXYZ.rowCount() != 1){
        pLogger->ERREUR("Le nombre de réponses ne correspond pas. ");
        return false;
    }

   QString idsGeom = GeomFromSXYZ.record(0).value("GEOM").toString();


   //sélection des sommets voisins ------------------------------------------------------------------------------

   QSqlQueryModel GeomVoisFromSXYZ;

   QSqlQuery queryVoisSxyz;
   queryVoisSxyz.prepare("SELECT IDS FROM SXYZ WHERE ST_Distance(GEOM, :IDSGEOM) < :BUFFER ;");
   queryVoisSxyz.bindValue(":IDSGEOM",idsGeom);
   queryVoisSxyz.bindValue(":BUFFER",buffer);

   GeomVoisFromSXYZ.setQuery(queryVoisSxyz);

   if (GeomVoisFromSXYZ.lastError().isValid()) {
       pLogger->ERREUR(QString("Impossible de recuperer des infos pour les voisins (SXYZ) : %1").arg(GeomVoisFromSXYZ.lastError().text()));
       return false;
   }//end if : test requete QSqlQueryModel

   //nombre de voisins
   int nbVois = GeomVoisFromSXYZ.rowCount();

   //tableau des voisins
   int idsVois[nbVois];
   for(int i = 0; i < nbVois; i++){
       //tableau des sommets voisin du sommet ids
       idsVois[i] = GeomFromSXYZ.record(i).value("IDS").toInt();
   }

   //tableau des arcs (tous !)
   float tabAzArcs[m_nbArcs + 1];
   for(int i = 0; i < m_nbArcs + 1; i++){
       //tableau des sommets voisin du sommet ids
       tabAzArcs[i] = -1.0;
   }

   //pour chaque voisin
   for(int v = 0; v < nbVois; v++){

       //sélection de l'azimut des arcs arrivant sur le sommet voisin ------------------------------------------------------------------------------
       //on rempli le tableau d'arcs avec les azimuths trouvés

       QSqlQueryModel AzimFromSIF_SI;

       QSqlQuery querySif_i;
       querySif_i.prepare("SELECT IDA, azimuth_i FROM SIF WHERE SI =  :IDS;");
       querySif_i.bindValue(":IDS",idsVois[v]);

       AzimFromSIF_SI.setQuery(querySif_i);

       if (AzimFromSIF_SI.lastError().isValid()) {
           pLogger->ERREUR(QString("Impossible de recuperer des infos pour les azimuths (SIF) : %1").arg(AzimFromSIF_SI.lastError().text()));
           return false;
       }//end if : test requete QSqlQueryModel

       //il peut y avoir plusieurs azimuts (plusieurs arcs) pour un sommet
       int nbAzim_i = AzimFromSIF_SI.rowCount();

       for(int az = 0; az < nbAzim_i; az++){

           int ida = AzimFromSIF_SI.record(az).value("IDA").toInt();

           if(tabAzArcs[ida] == -1){

               tabAzArcs[ida] = AzimFromSIF_SI.record(az).value("azimuth_i").toFloat();
           }
           else{ pLogger->ATTENTION(QString("deux azimuths trouvés pour l'arc : %1").arg(ida));}

       }//end for az

       QSqlQueryModel AzimFromSIF_SF;

       QSqlQuery querySif_f;
       querySif_f.prepare("SELECT IDA, azimuth_f FROM SIF WHERE SF =  :IDS;");
       querySif_f.bindValue(":IDS",idsVois[v]);

       AzimFromSIF_SF.setQuery(querySif_f);

       if (AzimFromSIF_SF.lastError().isValid()) {
           pLogger->ERREUR(QString("Impossible de recuperer des infos pour les azimuths (SIF) : %1").arg(AzimFromSIF_SF.lastError().text()));
           return false;
       }//end if : test requete QSqlQueryModel

       //il peut y avoir plusieurs azimuts (plusieurs arcs) pour un sommet
       int nbAzim_f = AzimFromSIF_SF.rowCount();

       for(int az = 0; az < nbAzim_f; az++){

           int ida = AzimFromSIF_SF.record(az).value("IDA").toInt();

           if(tabAzArcs[ida] == -1){

               tabAzArcs[ida] = AzimFromSIF_SF.record(az).value("azimuth_f").toFloat();
           }
            else{ pLogger->ATTENTION(QString("deux azimuths trouvés pour l'arc' : %1").arg(ida));}

       }//end for az



       //-->> on obtient un tableau, tabAzArcs où les lignes des ida des arcs voisins sont remplies avec l'azimuth des arcs



*/





       /*int idsVois[nbAzim];

       for(int i = 0; i < nbAzim; i++){
           //tableau des sommets voisin du sommet ids
           idsVois[i] = GeomFromSXYZ.record(i).value("IDS").toInt();
       }

    }//end for i*/







    /*QSqlQuery querySxyz;
    querySxyz.prepare("SELECT GEOM FROM SXYZ WHERE IDS = :IDS ;");
    querySxyz.bindValue(":IDS", ids);

    if (! querySxyz.exec()) {
        pLogger->ERREUR(QString("Récupération des infos (SXYZ) du sommet %1").arg(ids));
        return false;
    }

    querySxyz.next();

    QString idsGeom = querySxyz.record().value(0).toString();*/

    //recherche des arcs candidats

   /*QSqlQueryModel IdaFromAXYZ;

   QString queryAxyz = QString(("SELECT GEOM FROM SXYZ WHERE IDS = %1 ;").arg(ids));

   GeomFromSXYZ.setQuery(querySxyz);

   if (GeomFromSXYZ.lastError().isValid()) {
       pLogger->ERREUR(QString("Impossible de recuperer des infos (SXYZ) : %1").arg(GeomFromSXYZ->lastError().text()));
       return false;
   }//end if : test requete QSqlQueryModel

   if(GeomFromSXYZ.rowCount() != 1){
       pLogger->ERREUR("Le nombre de réponses ne correspond pas. ");
       return false;
   }

   QString idsGeom = GeomFromSXYZ.record(0).value("GEOM").toString();

    QSqlQuery queryAxyz;
    queryAxyz.prepare("SELECT IDA, ST_Distance(GEOM, :IDSGEOM) FROM AXYZ WHERE ST_Distance(GEOM, :IDSGEOM) < :D ;");
    queryAxyz.bindValue(":IDSGEOM", idsGeom);
    queryAxyz.bindValue(":D", seuil);

    if (! queryAxyz.exec()) {
        pLogger->ERREUR(QString("Récupération des infos (AXYZ) du sommet %1").arg(ids));
        return false;
    }

    //nombre de résulats :

    queryAxyz.next();

    *ida = queryAxyz.record().value(0).toInt();
    *dist = queryAxyz.record().value(1).toFloat();

    QSqlQuery querySif;
    querySif.prepare("SELECT azimuth_i, azimuth_f FROM SIF WHERE IDA = :IDA ;");
    querySif.bindValue(":IDA", *ida);

    if (! querySif.exec()) {
        pLogger->ERREUR(QString("Récupération des infos (SIF) du sommet %1").arg(ids));
        return false;
    }

    querySif.next();

    *azi = querySif.record().value(0).toFloat();
    *azf = querySif.record().value(1).toFloat();*/

/*    return true;
}*/

double Graphe::getAngle(int ids, int ida1, int ida2) {
    QSqlQuery queryAngle;
    queryAngle.prepare("SELECT ANGLE FROM ANGLES WHERE (IDS = :IDS) AND ((IDA1 = :IDARC11 AND IDA2 = :IDARC21)  OR (IDA2 = :IDARC12 AND IDA1 = :IDARC22));");

    queryAngle.bindValue(":IDARC11",ida1);
    queryAngle.bindValue(":IDARC21",ida2);
    queryAngle.bindValue(":IDARC12",ida1);
    queryAngle.bindValue(":IDARC22",ida2);
    queryAngle.bindValue(":IDS",ids);

    if (! queryAngle.exec()) {
        pLogger->ERREUR(QString("Récupération de l'angle entre les arcs %1 et %2 : %3").arg(ida1).arg(ida2).arg(queryAngle.lastError().text()));
        return -1;
    }

    double angle = -1;
    int nb_res = 0;

    if (queryAngle.next()) {

        angle = queryAngle.record().value(0).toDouble();
        nb_res++;

        while(queryAngle.next()){
            pLogger->INFO(QString("angle trouve entre les arcs %1 et %2 au sommet %3 : %4").arg(ida1).arg(ida2).arg(ids).arg(angle));
            pLogger->INFO(QString("angle trouve entre les arcs %1 et %2 au sommet %3 : %4").arg(ida1).arg(ida2).arg(ids).arg(queryAngle.record().value(0).toDouble()));

            nb_res ++;
            if(queryAngle.record().value(0).toDouble() < angle){
                angle = queryAngle.record().value(0).toDouble();
            }
        }

        if(nb_res != 1){
            pLogger->ATTENTION(QString("NOMBRE DE RESULTATS TROUVES entre les arcs %1 et %2 au sommet %3 : %4 - ANGLE CHOISI : %5").arg(ida1).arg(ida2).arg(ids).arg(nb_res).arg(angle));
        }

    } else {
        pLogger->ERREUR(QString("Pas d'angle trouvé entre les arcs %1 et %2 au sommet %3").arg(ida1).arg(ida2).arg(ids));
        return -1;
    }

    if (queryAngle.next()) {
        pLogger->ATTENTION(QString("Plusieurs angles trouvés entre les arcs %1 et %2 (arc boucle) au sommet %3").arg(ida1).arg(ida2).arg(ids));
        //return -1;
    }

    return angle;
}


bool Graphe::checkAngle(int ids, int ida1, int ida2) {
    QSqlQuery queryAngle;
    queryAngle.prepare("UPDATE ANGLES SET USED=TRUE WHERE (IDS = :IDS) AND ((IDA1 = :IDARC11 AND IDA2 = :IDARC21) OR (IDA2 = :IDARC12 AND IDA1 = :IDARC22));");

    queryAngle.bindValue(":IDARC11",ida1);
    queryAngle.bindValue(":IDARC21",ida2);
    queryAngle.bindValue(":IDARC12",ida1);
    queryAngle.bindValue(":IDARC22",ida2);
    queryAngle.bindValue(":IDS",ids);

    if (! queryAngle.exec()) {
        pLogger->ERREUR(QString("Mise à jour de l'angle (USED) entre les arcs %1 et %2 au sommet %3 : %4").arg(ida1).arg(ida2).arg(ids).arg(queryAngle.lastError().text()));
        return false;
    }

    return true;
}
