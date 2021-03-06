#include "Voies.h"

#include <iostream>
#include <fstream>
using namespace std;

#include <math.h>
#define PI 3.1415926535897932384626433832795

namespace WayMethods {

    int numMethods = 4;

    QString MethodeVoies_name[4] = {
        "Choix des couples par angles minimum",
        "Choix des couples par somme minimale des angles",
        "Choix des couples aleatoire",
        "Une voie = un arc"
    };
}

Voies::Voies(Database* db, Logger* log, Graphe* graphe, WayMethods::methodID methode, double seuil)
{
    pDatabase = db;
    pLogger = log;

    pLogger->INFO(QString("Methode de contruction des voies : %1").arg(WayMethods::MethodeVoies_name[methode]));

    m_Graphe = graphe;
    m_seuil_angle = seuil;
    m_methode = methode;
    m_nbCouples = 0;
    m_nbCelibataire = 0;
    m_nbVoies = 0;
    m_length_tot = 0;

    // Les vecteurs dont on connait deja la taille sont dimensionnes
    m_Couples.resize(graphe->getNombreArcs() + 1);
    m_ArcVoies.resize(graphe->getNombreArcs() + 1);
    m_SomVoies.resize(graphe->getNombreSommets() + 1);

    // Les vecteurs qui stockerons les voies n'ont pas une taille connue
    // On ajoute a la place d'indice 0 un vecteur vide pour toujours avoir :
    // indice dans le vecteur = identifiant de l'objet en base
    m_VoieArcs.push_back(QVector<long>(0));
    m_VoieSommets.push_back(QVector<long>(0));
}

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_COUPLES
//
//***************************************************************************************************************************************************

bool Voies::findCouplesAngleMin(int ids)
{
    QVector<long> stayingArcs;
    for (int a = 0; a < m_Graphe->getArcsOfSommet(ids)->size(); a++) {
        stayingArcs.push_back(m_Graphe->getArcsOfSommet(ids)->at(a));
    }

    QSqlQueryModel modelAngles;
    QSqlQuery queryAngles;
    queryAngles.prepare("SELECT IDA1, IDA2, ANGLE FROM ANGLES WHERE IDS = :IDS ORDER BY ANGLE ASC;");
    queryAngles.bindValue(":IDS",ids);

    if (! queryAngles.exec()) {
        pLogger->ERREUR(QString("Recuperation arc dans findCouplesAngleMin : %1").arg(queryAngles.lastError().text()));
        return false;
    }

    modelAngles.setQuery(queryAngles);

    pLogger->DEBUG(QString("%1 angles").arg(modelAngles.rowCount()));

    for (int ang = 0; ang < modelAngles.rowCount(); ang++) {

        if (modelAngles.record(ang).value("ANGLE").toDouble() > m_seuil_angle) {
            // A partir de maintenant tous les angles seront au dessus du seuil
            // Tous les arcs qui restent sont celibataires
            for (int a = 0; a < stayingArcs.size(); a++) {
                long ida = stayingArcs.at(a);
                m_Couples[ida].push_back(ids);
                m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
                m_Impasses.push_back(ida); // Ajout comme impasse
                m_nbCelibataire++;
            }
            return true;
        }

        int a1 = modelAngles.record(ang).value("IDA1").toInt();
        int a2 = modelAngles.record(ang).value("IDA2").toInt();

        int occ1 = stayingArcs.count(a1);
        int occ2 = stayingArcs.count(a2);

        if (occ1 == 0 || occ2 == 0) continue; // Un des arcs a deja ete traite
        if (a1 == a2 && occ1 < 2) continue; // arc boucle deja utilise, mais est en double dans le tableau des arcs

        m_Couples[a1].push_back(ids);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(ids);
        m_Couples[a2].push_back(a1);

        m_nbCouples++;

        int idx = stayingArcs.indexOf(a1);
        if (idx == -1) {
            pLogger->ERREUR("Pas normal de ne pas avoir l'arc encore disponible");
            return false;
        }
        stayingArcs.remove(idx);
        idx = stayingArcs.indexOf(a2);
        if (idx == -1) {
            pLogger->ERREUR("Pas normal de ne pas avoir l'arc encore disponible");
            return false;
        }
        stayingArcs.remove(idx);
    }

    if (stayingArcs.size() > 1) {
        pLogger->ERREUR("Pas normal d'avoir encore plus d'un arc a cet endroit");
        return false;
    }

    if (! stayingArcs.isEmpty()) {
        pLogger->DEBUG("Un celibataire !!");
        // On a un nombre impair d'arcs, donc un arc celibataire
        long ida = stayingArcs.at(0);
        m_Couples[ida].push_back(ids);
        m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
        m_Impasses.push_back(ida); // Ajout comme impasse
        m_nbCelibataire++;
    }

    return true;
}


bool Voies::findCouplesAngleSommeMin(int ids, int N_couples, int pos_couple, int lsom, int pos_ut, QVector< QVector<bool> >* V_utArcs, QVector< QVector<double> >* V_sommeArc, int nb_possib){

    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);
    int N_arcs = arcs->size();

    if(pos_couple>N_couples){//ON A FINI LE REMPLISSAGE

        //cout<<"DERNIER ARC"<<endl;

        if(N_arcs%2 == 1){//CAS NB IMPAIR D'ARCS

            for(int r=0; r<N_arcs; r++){

                if (V_utArcs->at(r).at(pos_ut) == false && V_sommeArc->at(lsom).last() == -1){//CAS ARC NON UTILISE

                    (*V_sommeArc)[lsom].last() = arcs->at(r);

                    break;

                }//end if ARC NON UTILISE

            }//end for r


        }//end if IMPAIR

        //TEST
        if(V_sommeArc->at(lsom).last() == -1){
            pLogger->ERREUR(QString("nombre d'arcs insuffisant pour la somme : %1 sur la ligne %2").arg(V_sommeArc->at(lsom)[0]).arg(lsom));
            return false;
        }//end if nbArcs_utilises != N_arcs

        else if(lsom == nb_possib-1){//TOUT A ETE REMPLI
            return true;

        }//end else if

        //TOUT EST OK, ON PASSE A LA LIGNE SUIVANTE

    }
    else{

        //ARC I ----------------------------------------------------------------------
        for(int i=0; i<N_arcs; i++){

            if(! V_utArcs->at(i)[pos_ut]){

                int arc_i = arcs->at(i);

                //ARC J --------------------------------------------------------------
                for(int j=i+1; j<N_arcs; j++){

                    if(! V_utArcs->at(j)[pos_ut]) {

                        int arc_j = arcs->at(j);

                        //STOCKAGE ---------------------------------------------------

                        if(V_sommeArc->at(lsom)[2*pos_couple-1] == -1 && V_sommeArc->at(lsom)[2*pos_couple] == -1){//betonnage
                            double angle = m_Graphe->getAngle(ids, arc_i, arc_j);
                            if (angle < 0) return false;
                            (*V_sommeArc)[lsom][0] += angle;
                            (*V_sommeArc)[lsom][2*pos_couple-1] = arc_i;
                            (*V_sommeArc)[lsom][2*pos_couple] = arc_j;

                            //MEMOIRE DES COUPLES DEJA UTILISES --------------------------
                            for(int r=pos_ut+1; r<N_couples+1; r++){
                                (*V_utArcs)[i][r] = true;
                                (*V_utArcs)[j][r] = true;
                            }//end for reste

                        }//end if betonnage


                        //COMPLETION DES COLONNES PRECEDENTES DE LA LIGNE
                        for(int p=1; p<(2*pos_couple-1); p+=2){
                            if(V_sommeArc->at(lsom)[p] == -1){//NON REMPLI

                                int arc_p=V_sommeArc->at(lsom-1)[p];
                                int arc_p1=V_sommeArc->at(lsom-1)[p+1];

                                double angle = m_Graphe->getAngle(ids, arc_p, arc_p1);
                                if (angle < 0) return false;
                                (*V_sommeArc)[lsom][0] += angle;

                                (*V_sommeArc)[lsom][p] = V_sommeArc->at(lsom-1)[p];
                                (*V_sommeArc)[lsom][p+1] = V_sommeArc->at(lsom-1)[p+1];

                                //MEMOIRE DES COUPLES DEJA UTILISES --------------------------
                                for(int c=0; c<N_arcs; c++){
                                    if(arcs->at(c) == V_sommeArc->at(lsom)[p] || arcs->at(c) == V_sommeArc->at(lsom)[p+1]){
                                        for(int r=pos_ut+1; r<N_couples+1; r++){
                                            (*V_utArcs)[c][r] = true;
                                        }//end for reste
                                    }//end if
                                }//end for c


                            }//end if
                        }//end for completion


                        //NOUVELLE RECHERCHE
                        findCouplesAngleSommeMin(ids, N_couples, pos_couple+1, lsom, pos_ut+1, V_utArcs, V_sommeArc, nb_possib);

                        //MISE A JOUR DE LSOM
                        for(int t=0; t<V_sommeArc->size(); t++){
                            if(V_sommeArc->at(t).back() == -1){
                                //cout<<"lsom reevaluee : "<<t<<endl;
                                lsom = t;
                                break;
                            }//end if
                        }//end for t

                        //MISE A JOUR DE V_UTARCS
                        //REMISE A 0 POUR LES NIVEAUX SUPERIEURS
                        for(int t = 0; t < V_sommeArc->size()-1; t++){
                            if(V_sommeArc->at(t).back() != -1 && V_sommeArc->at(t+1)[0] == 0){//on commence une nouvelle ligne
                                for(int i=0; i<N_arcs; i++){
                                    for(int j=pos_ut+1; j<N_couples+1; j++){
                                        (*V_utArcs)[i][j] = false;
                                    }//end for j
                                }//end for i
                                //cout<<"i et j remis a 0"<<endl;
                                break;
                            }//end if
                        }//end for t


                    }//end if (!arcs_utilises.at(j))

                }//end for j

             }//end if (!arcs_utilises.at(i))

        }//end for i

    }//end else (pos_couple>N_couples)

    //cout<<"-- FIN -- "<<pos_ut<<endl;

    return true;

}//END

// On ne fait pas vraiment un rando, on prend juste les arcs dans l'ordre, sans verifier les angles.
bool Voies::findCouplesRandom(int ids)
{
    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);
    int a = 0;
    while (a+1 < arcs->size()) {
        long a1 = arcs->at(a);
        long a2 = arcs->at(a+1);

        double angle = m_Graphe->getAngle(ids, a1, a2);
        if (angle < 0) {
            pLogger->ERREUR(QString("Impossible de trouver l'angle entre les arcs %1 et %2").arg(a1).arg(a2));
            return false;
        }

        if (angle < m_seuil_angle) {
            m_Couples[a1].push_back(ids);
            m_Couples[a1].push_back(a2);
            m_Couples[a2].push_back(ids);
            m_Couples[a2].push_back(a1);
            m_nbCouples++;
        } else {
            // On fait des 2 arcs 2 impasses
            m_Couples[a1].push_back(ids);
            m_Couples[a1].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
            m_Impasses.push_back(a1); // Ajout comme impasse
            m_nbCelibataire++;

            m_Couples[a2].push_back(ids);
            m_Couples[a2].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
            m_Impasses.push_back(a2); // Ajout comme impasse
            m_nbCelibataire++;
        }
        a += 2;
    }

    if (a < arcs->size() ) {
        // On a un nombre impair d'arcs, donc un arc celibataire
        long ida = arcs->at(a);
        m_Couples[ida].push_back(ids);
        m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
        m_Impasses.push_back(ida); // Ajout comme impasse
        m_nbCelibataire++;
    }

    return true;
}

// On ne fait un couple que si le sommet est de degres 2
bool Voies::findCouplesArcs(int ids)
{
    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);

    if (arcs->size() == 2){

        long a1 = arcs->at(0);
        long a2 = arcs->at(1);

        m_Couples[a1].push_back(ids);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(ids);
        m_Couples[a2].push_back(a1);
        m_nbCouples++;

    }
    else{

        for (int i=0; i<arcs->size(); i++){
             long a = arcs->at(i);

             m_Couples[a].push_back(ids);
             m_Couples[a].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
             m_Impasses.push_back(a); // Ajout comme impasse
             m_nbCelibataire++;
        }

    }

    return true;
}// end findArcs


// On fait un couple en fonction de la direction des arcs
/*bool Voies::findCouplesAzimut(int ids, int buffer)
{
    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);

    if (arcs->size() == 2){

        long a1 = arcs->at(0);
        long a2 = arcs->at(1);

        m_Couples[a1].push_back(ids);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(ids);
        m_Couples[a2].push_back(a1);
        m_nbCouples++;

    }
    else{

        for (int i=0; i<arcs->size(); i++){
             long a = arcs->at(i);

             m_Couples[a].push_back(ids);
             m_Couples[a].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
             m_Impasses.push_back(a); // Ajout comme impasse
             m_nbCelibataire++;
        }

    }

    return true;
}// end findCouplesAzimut*/

bool Voies::buildCouples(int buffer){

    pLogger->INFO("------------------------- buildCouples START -------------------------");

    //open the file
    ofstream degreefile;
    degreefile.open("degree.txt");

    for(int ids = 1; ids <= m_Graphe->getNombreSommets(); ids++){
        pLogger->DEBUG(QString("Le sommet IDS %1").arg(ids));

        QVector<long>* arcsOfSommet = m_Graphe->getArcsOfSommet(ids);

        //nombre d'arcs candidats
        int N_arcs = arcsOfSommet->size();

        pLogger->DEBUG(QString("Le sommet IDS %1 se trouve sur %2 arc(s)").arg(ids).arg(N_arcs));
        for(int a = 0; a < N_arcs; a++){
            pLogger->DEBUG(QString("     ID arcs : %1").arg(arcsOfSommet->at(a)));
        }//endfora

        //ECRITURE DU DEGRES DES SOMMETS
        //writing
        degreefile << ids;
        degreefile << " ";
        degreefile << N_arcs;
        degreefile << endl;



        //INITIALISATION

        if (N_arcs > 0) {

            //CALCUL DES COUPLES

            //si 1 seul arc passe par le sommet
            if(N_arcs == 1){
                long ida = arcsOfSommet->at(0);
                m_Couples[ida].push_back(ids);
                m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
                m_Impasses.push_back(ida); // Ajout comme impasse
                m_nbCelibataire++;

                continue;
            }//endif 1

            //si 2 arcs passent par le sommet : ils sont ensemble !
            if(N_arcs == 2){
                long a1 = arcsOfSommet->at(0);
                long a2 = arcsOfSommet->at(1);

                m_Couples[a1].push_back(ids);
                m_Couples[a1].push_back(a2);
                m_Couples[a2].push_back(ids);
                m_Couples[a2].push_back(a1);
                m_nbCouples++;

                continue;
            }//endif 1

            switch(m_methode) {

                case WayMethods::ANGLE_MIN:
                    if (! findCouplesAngleMin(ids)) return false;
                    break;

                case WayMethods::ANGLE_SOMME_MIN:
                    {
                    //nombre de possibilites
                    int N_couples = N_arcs / 2;
                    float nb_possib=1;
                    int N = N_arcs;
                    while(N>2){
                        nb_possib = nb_possib * ((N*(N-1))/2);
                        N -= 2;
                    }//end while

                    //VECTEUR INDIQUANT SI L'ARC A DEJA ETE UTILISE OU PAS
                    QVector< QVector<bool> >* V_utArcs = new QVector<  QVector<bool> >(N_arcs);

                    for(int i=0; i<N_arcs; i++){
                        (*V_utArcs)[i].resize(N_couples+1);
                        for(int j=0; j<(*V_utArcs)[i].size(); j++){
                            (*V_utArcs)[i][j]=false;
                        }//end for j
                    }//end for i

                    //position dans ce vecteur (colonne)
                    int pos_ut = 0;

                    int pos_couple = 1;
                    int lsom = 0;

                    //VECTEUR DE SOMMES ET D'ARCS
                    QVector< QVector<double> >* V_sommeArc = new QVector<  QVector<double> >(nb_possib);
                    for(int i=0; i<V_sommeArc->size(); i++){
                        (*V_sommeArc)[i].resize(N_arcs+1);
                        (*V_sommeArc)[i][0]=0;
                        for(int j=1; j<V_sommeArc->at(i).size(); j++){
                            (*V_sommeArc)[i][j] = -1;
                        }//end for j
                    }//end for i


                    //RECHERCHE DES PAIRES
                    if (! findCouplesAngleSommeMin(ids, N_couples, pos_couple, lsom, pos_ut, V_utArcs, V_sommeArc, nb_possib)) {
                        return false;
                    }

                    //RESULTAT POUR LE SOMMET S (IDS = S+1)

                    double somme_min = N_couples*m_seuil_angle;
                    int lsom_min = 0;

                    //pour chaque somme calculee
                    for(int lsom = 0; lsom < V_sommeArc->size(); lsom++){

                        // SOMME = V_sommeArc[lsom][0]

                        if(V_sommeArc->at(lsom)[0] < somme_min){

                            lsom_min = lsom;
                            somme_min = V_sommeArc->at(lsom)[0];

                        }//endif nouvelle somme min

                    }//end for lsom

                    if(somme_min < N_couples*m_seuil_angle){

                        for(int i=1; i < V_sommeArc->at(lsom_min).size()-1; i+=2){
                            int a1 = V_sommeArc->at(lsom_min).at(i);
                            int a2 = V_sommeArc->at(lsom_min).at(i+1);

                            m_Couples[a1].push_back(ids);
                            m_Couples[a1].push_back(a2);
                            m_Couples[a2].push_back(ids);
                            m_Couples[a2].push_back(a1);

                            m_nbCouples++;

                        }//end for a

                        if (N_arcs%2 == 1) {
                            int a = V_sommeArc->at(lsom_min).last();
                            m_Couples[a].push_back(ids);
                            m_Couples[a].push_back(0);
                            m_Impasses.push_back(a);
                            m_nbCelibataire++;
                        }

                    } else {
                        QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);

                        for(int i=0; i<N_arcs; i++){

                            int a = arcs->at(i);
                            m_Couples[a].push_back(ids);
                            m_Couples[a].push_back(0);
                            m_Impasses.push_back(a);
                            m_nbCelibataire++;
                        }//end for i

                    }

                    break;
                    }

                case WayMethods::ANGLE_RANDOM:
                    if (! findCouplesRandom(ids)) return false;
                    break;

                case WayMethods::ARCS:
                    if (! findCouplesArcs(ids)) return false;
                    break;

                //case WayMethods::AZIMUT:
                //    if (! findCouplesAzimut(ids, buffer)) return false;
                //    break;

                default:
                    pLogger->ERREUR("Methode d'appariement des arcs inconnu");
                    return false;
            }

        } else{
            pLogger->ERREUR(QString("Le sommet %1 n'a pas d'arcs, donc pas de couple").arg(ids));
            return false;
        }//end else

    }//end for s

    //close the file
    degreefile.close();

    //NOMBRE DE COUPLES
    pLogger->INFO(QString("Nb TOTAL de Couples : %1").arg(m_nbCouples));
    pLogger->INFO(QString("Nb TOTAL de Célibataires : %1").arg(m_nbCelibataire));

    pLogger->INFO("-------------------------- buildCouples END --------------------------");

    // Tests de m_Couples
    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 4) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 4)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
        pLogger->DEBUG(QString("Arc %1").arg(a));
        pLogger->DEBUG(QString("    Arcs couples : %1 et %2").arg(m_Couples.at(a).at(1)).arg(m_Couples.at(a).at(3)));
    }

    return true;

}//END build_couples

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DES VECTEURS : M_VOIE_ARCS
//
//***************************************************************************************************************************************************

bool Voies::buildVectors(){

    pLogger->INFO("------------------------- buildVectors START -------------------------");

    // On parcourt la liste des impasses pour constituer les voies
    for (int i = 0; i < m_Impasses.size(); i++) {

        int ida_precedent = 0;
        int ida_courant = m_Impasses.at(i);
        if (m_Couples.at(ida_courant).size() == 5) {
            // Cette impasse a deja ete traitee : la voie a deja ete ajoutee
            continue;
        }

        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc impasse %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ids_courant = 0;
        if (m_Couples.at(ida_courant).at(1) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(0);
        } else if (m_Couples.at(ida_courant).at(3) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(2);
        } else {
            pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 doit etre une impasse").arg(ida_courant));
            return false;
        }

        voieS.push_back(ids_courant);
        m_SomVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 est deja utilise par la voie %2").arg(ida_courant).arg(m_Couples.at(ida_courant).at(4)));
                return false;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            voieS.push_back(ids_courant);
            m_SomVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoieSommets.push_back(voieS);
        m_nbVoies++;
    }

    // Nous avons maintenant traite toutes les impasses. Ils restent potentiellement des arcs a traiter (le cas des boucles)
    for (int ida = 1; ida < m_Couples.size(); ida++) {
        if (m_Couples.at(ida).size() == 5) continue;

        int ida_courant = ida;
        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ida_precedent = m_Couples.at(ida_courant).at(1);
        int ids_courant = m_Couples.at(ida_courant).at(0);

        voieS.push_back(ids_courant);
        m_SomVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->DEBUG("On est retombe sur un arc deja utilise : la boucle est bouclee");
                break;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            voieS.push_back(ids_courant);

            m_SomVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoieSommets.push_back(voieS);
        m_nbVoies++;
    }

    pLogger->INFO(QString("Nombre de voies : %1").arg(m_nbVoies));

    // CONSTRUCTION DE VOIE-VOIES
    m_VoieVoies.resize(m_nbVoies + 1);
    for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++) {

        //on parcours les sommets sur la voie
        for(int s = 0; s < m_VoieSommets[idv1].size(); s++){

            long ids = m_VoieSommets[idv1][s];

            //on cherche les voies passant par ces sommets
            for(int v = 0; v < m_SomVoies[ids].size(); v++){

                long idv2 = m_SomVoies[ids][v];
                m_VoieVoies[idv1].push_back(idv2);

            }//end for v (voies passant par les sommets sur la voie)

        }//end for s (sommets sur la voie)

    }//end for idv1 (voie)

    pLogger->INFO("-------------------------- buildVectors END --------------------------");

    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 5) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 5)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
    }

    return true;

}//END build_VoieArcs


//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE VOIES EN BDD
//
//***************************************************************************************************************************************************

bool Voies::build_VOIES(){

    bool voiesToDo = true;

    if (! pDatabase->tableExists("VOIES")) {
        voiesToDo = true;
        pLogger->INFO("La table des VOIES n'est pas en base, on la construit !");
    } else {
        // Test si la table VOIES a deja ete construite avec les memes parametres methode + seuil
        QSqlQueryModel estMethodeActive;
        estMethodeActive.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2 AND ACTIVE=TRUE").arg((int) m_methode).arg((int) m_seuil_angle));
        if (estMethodeActive.lastError().isValid()) {
            pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de ce methode + seuil : %1").arg(estMethodeActive.lastError().text()));
            return false;
        }

        if (estMethodeActive.rowCount() > 0) {
            pLogger->INFO("La table des VOIES est deja faite avec cette methode et ce seuil des angles");
            voiesToDo = false;
        } else {
            pLogger->INFO("La table des VOIES est deja faite mais pas avec cette methode ou ce seuil des angles : on la refait");
            // Les voies existent mais n'ont pas ete faites avec la meme methode
            voiesToDo = true;

            if (! pDatabase->dropTable("VOIES")) {
                pLogger->ERREUR("Impossible de supprimer VOIES, pour la recreer avec la bonne methode");
                return false;
            }
        }
    }

    //SUPPRESSION DE LA TABLE VOIES SI DEJA EXISTANTE
    if (voiesToDo) {

        pLogger->INFO("-------------------------- build_VOIES START ------------------------");

        // MISE A JOUR DE USED DANS ANGLES

        QSqlQuery queryAngle;
        queryAngle.prepare("UPDATE ANGLES SET USED=FALSE;");

        if (! queryAngle.exec()) {
            pLogger->ERREUR(QString("Mise à jour de l'angle (USED=FALSE) : %1").arg(queryAngle.lastError().text()));
            return false;
        }

        for (int a = 1; a < m_Couples.size(); a++) {

            int som1_couple = m_Couples.at(a).at(0);
            int a1 = m_Couples.at(a).at(1);

            if (a1 != 0) {
                if (! m_Graphe->checkAngle(som1_couple, a, a1)) return false;
            }

            int som2_couple = m_Couples.at(a).at(2);
            int a2 = m_Couples.at(a).at(3);
            if (a2 != 0) {
                if (! m_Graphe->checkAngle(som2_couple, a, a2)) return false;
            }
        }

        //CREATION DE LA TABLE VOIES D'ACCUEIL DEFINITIVE

        QSqlQueryModel createVOIES;
        createVOIES.setQuery("CREATE TABLE VOIES ( IDV SERIAL NOT NULL PRIMARY KEY, MULTIGEOM geometry, LENGTH float, NBA integer, NBS integer, NBC integer, NBC_P integer);");

        if (createVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_voies : %1").arg(createVOIES.lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //RECONSTITUTION DE LA GEOMETRIE DES VOIES

        QSqlQueryModel *geometryArcs = new QSqlQueryModel();

        geometryArcs->setQuery("SELECT IDA AS IDA, GEOM AS GEOM FROM AXYZ;");

        if (geometryArcs->lastError().isValid()) {
            pLogger->ERREUR(QString("req_arcsvoies : %1").arg(geometryArcs->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        int NA = m_Graphe->getNombreArcs();

        if(geometryArcs->rowCount() != NA){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE D'ARCS");
            return false;
        }

        QVector<QString> geometryVoies(m_nbVoies + 1);

        //pour toutes les voies
        for (int i = 0; i < geometryArcs->rowCount(); i++) {
            int ida = geometryArcs->record(i).value("IDA").toInt();
            QString geom = geometryArcs->record(i).value("GEOM").toString();
            long idv = m_Couples.at(ida).at(4);
            if (geometryVoies[idv].isEmpty()) {
                geometryVoies[idv] = "'" + geom + "'";
            } else {
                geometryVoies[idv] += ", '" + geom + "'";
            }
        }

        delete geometryArcs;


        ofstream geomfile;
        geomfile.open("geom.txt");


        // On ajoute toutes les voies, avec la geometrie (multi), le nombre d'arcs, de sommets et la connectivite
        for (int idv = 1; idv <= m_nbVoies; idv++) {

            int nba = m_VoieArcs.at(idv).size();
            int nbs = m_VoieSommets.at(idv).size();

            // CALCUL DU NOMBRE DE CONNEXION DE LA VOIE
            int nbc = 0;
            int nbc_p = 0;

            // Pour chaque sommet appartenant a la voie, on regarde combien d'arcs lui sont connectes
            // On enleve les 2 arcs correspondant a la voie sur laquelle on se trouve
            for(int s = 0;  s < m_VoieSommets[idv].size(); s++){
                nbc += m_Graphe->getArcsOfSommet(m_VoieSommets[idv][s])->size()-2;
            }//end for

            // Dans le cas d'une non boucle, on a enleve a tord 2 arcs pour les fins de voies
            if ( nba + 1 == nbs ) { nbc += 2; nbc_p = nbc - 2; }
            else { nbc_p = nbc;}


            geomfile << idv;
            geomfile << " ";
            geomfile << geometryVoies.at(idv).toStdString();
            geomfile << endl;



            QString addVoie = QString("INSERT INTO VOIES(IDV, MULTIGEOM, NBA, NBS, NBC, NBC_P) VALUES (%1, ST_LineMerge(ST_Union(ARRAY[%2])) , %3, %4, %5, %6);")
                    .arg(idv).arg(geometryVoies.at(idv)).arg(nba).arg(nbs).arg(nbc).arg(nbc_p);

            QSqlQuery addInVOIES;
            addInVOIES.prepare(addVoie);

            if (! addInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter la voie %1 dans la table VOIES : %2").arg(idv).arg(addInVOIES.lastError().text()));
                return false;
            }

        }

        geomfile.close();

        // On calcule la longueur des voies
        QSqlQuery updateLengthInVOIES;
        updateLengthInVOIES.prepare("UPDATE VOIES SET LENGTH = ST_Length(MULTIGEOM);");

        if (! updateLengthInVOIES.exec()) {
            pLogger->ERREUR(QString("Impossible de calculer la longueur de la voie dans la table VOIES : %1").arg(updateLengthInVOIES.lastError().text()));
            return false;
        }

        // On calcule la connectivite sur la longueur
        if (! pDatabase->add_att_div("VOIES","LOC","LENGTH","NBC")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_NBC", "NBC", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_LOC", "LOC", 10, true)) return false;


        pLogger->INFO("--------------------------- build_VOIES END --------------------------");
    } else {
        pLogger->INFO("------------------------- VOIES already exists -----------------------");
    }

    return true;
}

bool Voies::arcInVoie(long ida, long idv){

    //on parcourt tous les arcs de la voie
    for (int a2 = 0; a2 < m_VoieArcs.at(idv).size(); a2 ++){
        long ida2 = m_VoieArcs.at(idv).at(a2);

        if( ida2 == ida ){ return true; } //on a trouvé sur la voie l'arc en entrée

    }//end for

    return false;    //on n'a pas trouvé l'arc en entrée sur la voie

}//END arcInVoie


//***************************************************************************************************************************************************
//CALCUL DES ATTRIBUTS
//
//***************************************************************************************************************************************************

bool Voies::calcStructuralite(){

    if (! pDatabase->columnExists("VOIES", "RTOPO") || ! pDatabase->columnExists("VOIES", "STRUCT")) {
        pLogger->INFO("---------------------- calcStructuralite START ----------------------");

        // AJOUT DE L'ATTRIBUT DE STRUCTURALITE
        QSqlQueryModel addStructInVOIES;
        addStructInVOIES.setQuery("ALTER TABLE VOIES ADD RTOPO float, ADD STRUCT float;");

        if (addStructInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de structuralite dans VOIES : %1").arg(addStructInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *lengthFromVOIES = new QSqlQueryModel();

        lengthFromVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM VOIES;");

        if (lengthFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans VOIES : %1").arg(lengthFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if(lengthFromVOIES->rowCount() != m_nbVoies){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE DE VOIES ");
            return false;
        }

        //CREATION DU TABLEAU DONNANT LA LONGUEUR DE CHAQUE VOIE
        /*float length_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromVOIES->record(v).value("IDV").toInt();
            length_voie[idv]=lengthFromVOIES->record(v).value("LENGTH_VOIE").toFloat();
            m_length_tot += length_voie[idv];
        }//end for v*/

        //open the file
        ofstream lengthfile;
        lengthfile.open("length.txt");
        //lengthfile << "[";

        float length_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromVOIES->record(v).value("IDV").toInt();
            length_voie[idv]=lengthFromVOIES->record(v).value("LENGTH_VOIE").toFloat();

            //writing
            lengthfile << idv;
            lengthfile << " ";
            lengthfile << length_voie[idv];
            lengthfile << endl;

            m_length_tot += length_voie[idv];
        }//end for v

        //close the file
        //lengthfile << "]";
        lengthfile.close();

        //SUPPRESSION DE L'OBJET
        delete lengthFromVOIES;

        pLogger->INFO(QString("LONGUEUR TOTALE DU RESEAU : %1 metres").arg(m_length_tot));


        //TRAITEMENT DES m_nbVoies VOIES

        //open the file dtopofile
        ofstream dtopofile;
        dtopofile.open("dtopo.txt");
        //dtopofile << "[";

        //open the file adjacencyfile
        ofstream adjacencyfile;
        adjacencyfile.open("adjacency.txt");
        //adjacencyfile << "[";

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++){

            pLogger->DEBUG(QString("*** VOIE V : %1").arg(idv1));
            pLogger->DEBUG(QString("*** LENGTH : %1").arg(length_voie[idv1]));

            //dtopofile << idv1 << " ";
            //adjacencyfile << idv1 << " ";

            //INITIALISATION
            int structuralite_v = 0;
            int rayonTopologique_v = 0;
            int dtopo = 0;
            int nb_voiestraitees = 0;
            int nb_voiestraitees_test = 0;
            QVector<int> V_ordreNombre;
            QVector<int> V_ordreLength;
            int dtopo_voies[m_nbVoies + 1];

            for(int i = 0; i < m_nbVoies + 1; i++){
                dtopo_voies[i]=-1;
            }//end for i

            dtopo_voies[idv1] = dtopo;
            nb_voiestraitees = 1;

            V_ordreNombre.push_back(1);
            V_ordreLength.push_back(length_voie[idv1]);

           //-------------------

            //TRAITEMENT
            while(nb_voiestraitees != m_nbVoies){

                //cout<<endl<<"nb_voiestraitees : "<<nb_voiestraitees<<" / "<<m_nbVoies<<" voies."<<endl;
                nb_voiestraitees_test = nb_voiestraitees;

                //TRAITEMENT DE LA LIGNE ORDRE+1 DANS LES VECTEURS
                V_ordreNombre.push_back(0);
                V_ordreLength.push_back(0);

                //------------------------------------------voie j
                for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){
                    //on cherche toutes les voies de l'ordre auquel on se trouve
                    if(dtopo_voies[idv2] == dtopo){

                        for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {                            
                            long idv3 = m_VoieVoies.at(idv2).at(v2);

                            // = si la voie n'a pas deja ete traitee
                            if (dtopo_voies[idv3] == -1){

                                dtopo_voies[idv3] = dtopo +1;
                                nb_voiestraitees += 1;
                                V_ordreNombre[dtopo+1] += 1;
                                V_ordreLength[dtopo+1] += length_voie[idv3];

                            }//end if (voie non traitee)
                        }//end for v2

                    }//end if (on trouve les voies de l'ordre souhaite)

                }//end for j (voie)

                //si aucune voie n'a ete trouvee comme connectee a celle qui nous interesse
                if(nb_voiestraitees == nb_voiestraitees_test){
                    //cout<<"Traitement de la partie non connexe : "<<m_nbVoies-nb_voiestraitees<<" voies traitees / "<<m_nbVoies<<" voies."<<endl;


                    int nbvoies_connexe = 0;
                    for(int k=0; k<m_nbVoies; k++){
                        if(dtopo_voies[k]!=-1){
                            nbvoies_connexe += 1;
                        }
                        else{
                            nb_voiestraitees += 1;
                        }//end if
                    }//end for k

                    //SUPPRESSION DES VOIES NON CONNEXES AU GRAPHE PRINCIPAL

                    if(nbvoies_connexe < m_nbVoies/2){

                        QSqlQuery deleteVOIES;
                        deleteVOIES.prepare("DELETE FROM VOIES WHERE idv = :IDV ;");
                        deleteVOIES.bindValue(":IDV",idv1 );

                        if (! deleteVOIES.exec()) {
                            pLogger->ERREUR(QString("Impossible de supprimer la voie %1").arg(idv1));
                            pLogger->ERREUR(deleteVOIES.lastError().text());
                            return false;
                        }
                    }//end if nbvoies_connexe < m_nbVoies/2

                    break;

                }// end if nb_voiestraitees == nb_voiestraitees_test

                if(nb_voiestraitees == nb_voiestraitees_test){
                    pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2").arg(nb_voiestraitees).arg(m_nbVoies));
                    return false;
                }//end if

                dtopo += 1;

            }//end while (voies a traitees)


            //CALCUL DE LA STRUCTURALITE
            for(int l=0; l<V_ordreLength.size(); l++){

                structuralite_v += l * V_ordreLength.at(l);

            }//end for l (calcul de la simpliest centrality)


            //CALCUL DU RAYON TOPO
            for(int l=0; l<V_ordreNombre.size(); l++){

                rayonTopologique_v += l * V_ordreNombre.at(l);

            }//end for l (calcul de l'ordre de la voie)

            //INSERTION EN BASE
            QSqlQuery addStructAttInVOIES;
            addStructAttInVOIES.prepare("UPDATE VOIES SET RTOPO = :RT, STRUCT = :S WHERE idv = :IDV ;");
            addStructAttInVOIES.bindValue(":IDV",idv1 );
            addStructAttInVOIES.bindValue(":RT",rayonTopologique_v);
            addStructAttInVOIES.bindValue(":S",structuralite_v);

            if (! addStructAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer la structuralite %1 et le rayon topo %2 pour la voie %3").arg(structuralite_v).arg(rayonTopologique_v).arg(idv1));
                pLogger->ERREUR(addStructAttInVOIES.lastError().text());
                return false;
            }

            // Here we have the vector

            // dtopofile << "[";
            // adjacencyfile << "[";

            for(int i = 1; i < m_nbVoies + 1; i++){

                dtopofile << dtopo_voies[i];

                if(dtopo_voies[i] == -1 || dtopo_voies[i] == 0 || dtopo_voies[i] == 1){
                    adjacencyfile << dtopo_voies[i];
                }
                else if(dtopo_voies[i] > 1){
                    adjacencyfile << 0;
                }
                else{pLogger->ERREUR(QString("Erreur dans la matrice de distances topologiques"));}

                /*if(i != m_nbVoies){
                    dtopofile << ", ";
                    adjacencyfile << ", ";
                }
                else{*/
                    dtopofile << " ";
                    adjacencyfile << " ";
                //}

            }//end for i

           // dtopofile << "]";
           // adjacencyfile << "]";

            dtopofile << endl;
            adjacencyfile << endl;

        }//end for idv1

        //dtopofile << "]";
        dtopofile.close();

        //adjacencyfile << "]";
        adjacencyfile.close();

        if (! pDatabase->add_att_div("VOIES","SOL","STRUCT","LENGTH")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_S", "STRUCT", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_SOL", "SOL", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_RTOPO", "RTOPO", 10, true)) return false;

        if (! pDatabase->add_att_dif("VOIES", "DIFF_CL", "CL_S", "CL_RTOPO")) return false;

        pLogger->INFO("------------------------ calcStructuralite END ----------------------");
    } else {
        pLogger->INFO("---------------- Struct attributes already in VOIES -----------------");
    }

    return true;

}//END calcStructuralite

bool Voies::calcConnexion(){

    if (! pDatabase->columnExists("VOIES", "NBCSIN") || ! pDatabase->columnExists("VOIES", "NBCSIN_P")) {
        pLogger->INFO("---------------------- calcConnexion START ----------------------");

        // AJOUT DE L'ATTRIBUT DE CONNEXION
        QSqlQueryModel addNbcsinInVOIES;
        addNbcsinInVOIES.setQuery("ALTER TABLE VOIES ADD NBCSIN float, ADD NBCSIN_P float;");

        if (addNbcsinInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut nbcsin dans VOIES : %1").arg(addNbcsinInVOIES.lastError().text()));
            return false;
        }

        //pour chaque voie
        for(int idv = 1; idv <= m_nbVoies ; idv++){

            int nba = m_VoieArcs.at(idv).size();
            int nbs = m_VoieSommets.at(idv).size();
            float nbc_sin = 0;
            float nbc_sin_p = 0;

            //pondération de nbc par le SINUS de l'angle
            for (int i=0; i<m_VoieArcs[idv].size()-1; i++){

                int a1 = m_VoieArcs[idv][i];
                int a2 = m_VoieArcs[idv][i+1];
                int sommet = m_VoieSommets[idv][i+1];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1 || a3 == a2){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);
                    double ang2 = m_Graphe->getAngle(sommet, a2, a3);
                    double ang;

                    if (ang1 < 0 || ang2 < 0){
                        return false;
                    }

                    if (ang1 < ang2) ang = ang1;
                    else ang = ang2;

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                }

            }//end for


            //ETUDE DES SINUS EN FIN DE VOIE
            if (nba + 1 == nbs){ //on est PAS dans le cas d'une boucle

                float min_ang;
                float sin_angsmin;


                //POUR LE PREMIER SOMMET
                int a1 = m_VoieArcs[idv][0];
                int sommet = m_VoieSommets[idv][0];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                min_ang = arcs->at(0);
                sin_angsmin = 0;

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);

                    if (ang1 < 0){
                        return false;
                    }

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang1*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                    if (ang1 < min_ang){
                        min_ang = ang1;
                        sin_angsmin = sin_ang;
                    }
                }

                //ON RETIRE L'ARTEFACT D'ALIGNEMENT
                nbc_sin_p -= sin_angsmin;


                //POUR LE DERNIER SOMMET
                a1 = m_VoieArcs[idv].last();
                sommet = m_VoieSommets[idv].last();

                arcs = m_Graphe->getArcsOfSommet(sommet);

                min_ang = arcs->at(0);
                sin_angsmin = 0;

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1){
                        continue;
                    }


                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);

                    if (ang1 < 0){
                        return false;
                    }

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang1*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                    if (ang1 < min_ang){
                        min_ang = ang1;
                        sin_angsmin = sin_ang;
                    }
                }

                //ON RETIRE L'ARTEFACT D'ALIGNEMENT
                nbc_sin_p -= sin_angsmin;


            }//end if (nba + 1 == nbs)
            else{ //On est dans le cas d'une boucle

                int a1 = m_VoieArcs[idv][0];
                int a2 = m_VoieArcs[idv].last();
                int sommet = m_VoieSommets[idv][0];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1 || a3 == a2){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);
                    double ang2 = m_Graphe->getAngle(sommet, a2, a3);
                    double ang;

                    if (ang1 < 0 || ang2 < 0){
                        return false;
                    }

                    if (ang1 < ang2) ang = ang1;
                    else ang = ang2;

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                }


            }//end else (nba + 1 == nbs)

            nbc_sin_p += nbc_sin;

            if (nbc_sin_p < 0){
                pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (nbc_sin_p = %1)").arg(nbc_sin_p));
                return false;
            }

            //pLogger->ATTENTION(QString("nbc_sin_p : %1").arg(nbc_sin_p));
            //pLogger->ATTENTION(QString("idv : %1").arg(idv));
            //pLogger->ATTENTION(QString("nbc_sin : %1").arg(nbc_sin));

            //INSERTION EN BASE

            QString addNbcsin = QString("UPDATE VOIES SET NBCSIN = %1, NBCSIN_P = %2 WHERE idv = %3 ;").arg(nbc_sin).arg(nbc_sin_p).arg(idv);

            QSqlQuery addNbcsinAttInVOIES;
            addNbcsinAttInVOIES.prepare(addNbcsin);

            if (! addNbcsinAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter la voie %1 dans la table VOIES : %2").arg(idv).arg(addNbcsinAttInVOIES.lastError().text()));
                return false;
            }

        }//end for idv


        if (! pDatabase->add_att_div("VOIES", "CONNECT", "NBCSIN", "NBC")) return false;
        if (! pDatabase->add_att_div("VOIES", "CONNECT_P", "NBCSIN_P", "NBC_P")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_NBCSIN", "NBCSIN", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_CONNECT", "CONNECT", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_CONNECTP", "CONNECT_P", 10, true)) return false;

        //CLASSICATION EN 6 CLASSES
        // 0 - 0.6
        // 0.6 - 0.8
        // 0.8 - 0.9
        // 0.9 - 0.95
        // 0.95 - 0.98
        // 0.98 - max

        // AJOUT DE L'ATTRIBUT DE CLASSIF
        QSqlQueryModel addorthoInVOIES;
        addorthoInVOIES.setQuery("ALTER TABLE VOIES ADD C_ORTHO integer;");

        if (addorthoInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut c_ortho dans VOIES : %1").arg(addorthoInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *connectpFromVOIES = new QSqlQueryModel();

        connectpFromVOIES->setQuery("SELECT IDV, CONNECT_P FROM VOIES;");

        if (connectpFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les connect_p dans VOIES : %1").arg(connectpFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT L'ORTHOGONALITE DE CHAQUE VOIE
        float connectp_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){

            int idv = connectpFromVOIES->record(v).value("IDV").toInt();
            connectp_voie[idv] = connectpFromVOIES->record(v).value("CONNECT_P").toFloat();

            int c_connectp;

            if (connectp_voie[idv] < 0.6){c_connectp = 0;}
            else  if (connectp_voie[idv] < 0.8){c_connectp = 1;}
            else  if (connectp_voie[idv] < 0.9){c_connectp = 2;}
            else  if (connectp_voie[idv] < 0.95){c_connectp = 3;}
            else  if (connectp_voie[idv] < 0.98){c_connectp = 4;}
            else {c_connectp = 5;}

            //INSERTION EN BASE

            QString addOrtho = QString("UPDATE VOIES SET C_ORTHO = %1 WHERE idv = %2 ;").arg(c_connectp).arg(idv);

            QSqlQuery addOrthoAttInVOIES;
            addOrthoAttInVOIES.prepare(addOrtho);

            if (! addOrthoAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter pour la voie %1 , l'orhtogonalité : %2").arg(idv).arg(addOrthoAttInVOIES.lastError().text()));
                return false;
            }

        }//end for v




    } else {
        pLogger->INFO("---------------- Connect attributes already in VOIES -----------------");
    }

    return true;

}//END calcConnexion

bool Voies::calcUse(){

    if (! pDatabase->columnExists("VOIES", "USE") || ! pDatabase->columnExists("VOIES", "USE_MLT") || ! pDatabase->columnExists("VOIES", "USE_LGT")) {
        pLogger->INFO("---------------------- calcUse START ----------------------");

        // AJOUT DE L'ATTRIBUT DE USE
        QSqlQueryModel addUseInVOIES;
        addUseInVOIES.setQuery("ALTER TABLE VOIES ADD USE integer, ADD USE_MLT integer, ADD USE_LGT integer;");

        if (addUseInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de use dans VOIES : %1").arg(addUseInVOIES.lastError().text()));
            return false;
        }


        //CREATION DU TABLEAU COMPTEUR UNIQUE
        float voie_use[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_use[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR MULTIPLE
        float voie_useMLT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useMLT[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR DISTANCE
        float voie_useLGT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useLGT[v] = 0;
        }//end for v


        //CALCUL DU NOMBRE DE VOIES DE LA PARTIE CONNEXE
        QSqlQueryModel *nombreVOIES = new QSqlQueryModel();

        nombreVOIES->setQuery("SELECT COUNT(IDV) AS NBV FROM VOIES;");

        if (nombreVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de compter le nombre de VOIES : %1").arg(nombreVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if (nombreVOIES->rowCount() != 1) {
            pLogger->ERREUR(QString("Trop de réponses pour le nombre de voies : %1").arg(nombreVOIES->rowCount()));
            return false;
        }//end if : test requete QSqlQueryModel

        int nbVOIESconnexes = nombreVOIES->record(0).value("NBV").toInt();

        //SUPPRESSION DE L'OBJET
        delete nombreVOIES;


        //CREATION DU TABLEAU DONNANT LA LONGUEUR DE CHAQUE VOIE
        QSqlQueryModel *lengthFromVOIES = new QSqlQueryModel();

        lengthFromVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM VOIES;");

        if (lengthFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans VOIES : %1").arg(lengthFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float length_voie[m_nbVoies + 1];

        for(int v = 0; v < m_nbVoies + 1; v++){
            length_voie[v] = -1;
        }//end for v

        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromVOIES->record(v).value("IDV").toInt();
            length_voie[idv] = lengthFromVOIES->record(v).value("LENGTH_VOIE").toFloat();
            m_length_tot += length_voie[idv];
        }//end for v

        //SI length_voie[v] = -1 ici ça veut dire que la voie n'était pas connexe et a été retirée

        //SUPPRESSION DE L'OBJET
        delete lengthFromVOIES;



        //------------------------------------------voie i
        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++){

            if (length_voie[idv1] != -1){ //La voie idv1 existe dans la table VOIE, elle est connexe au reste

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur unique
                float voie_parente[m_nbVoies + 1];
                int idv_fille;
                int idv_parent;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parente[v] = -1;
                }//end for v
                voie_parente[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur multiple
                float voie_parenteMLT[m_nbVoies + 1];
                int idv_filleMLT;
                int idv_parentMLT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteMLT[v] = -1;
                }//end for v
                voie_parenteMLT[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur avec prise en compte de la longueur
                float voie_parenteLGT[m_nbVoies + 1];
                int idv_filleLGT;
                int idv_parentLGT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteLGT[v] = -1;
                }//end for v
                voie_parenteLGT[idv1] = 0;


                int dtopo = 0;
                int nb_voiestraitees = 0;
                int nb_voiestraitees_test = 0;

                //CREATION / MAJ DU TABLEAU donnant les distances topologiques de toutes les voies par rapport à la voie i
                int dtopo_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    dtopo_voies[i] = -1;
                }//end for i

                //CREATION / MAJ DU TABLEAU donnant les longueurs d'accès de toutes les voies par rapport à la voie i
                int lacces_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    lacces_voies[i]=0;
                }//end for i

                //traitement de la voie principale en cours (voie i)
                dtopo_voies[idv1] = dtopo;
                nb_voiestraitees = 1;


               while(nb_voiestraitees != nbVOIESconnexes){

                    nb_voiestraitees_test = nb_voiestraitees;

                    //------------------------------------------voie j
                    for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){

                        if (length_voie[idv2] != -1){ //La voie idv2 existe dans la table VOIE, elle est connexe au reste

                            //on cherche toutes les voies de l'ordre auquel on se trouve
                            if(dtopo_voies[idv2] == dtopo){

                                for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {
                                    long idv3 = m_VoieVoies.at(idv2).at(v2);

                                     if (length_voie[idv3] != -1){ //La voie idv3 existe dans la table VOIE, elle est connexe au reste

                                        //si on est dans le cas d'un premier chemin le plus cout ou d'un même chemin double
                                        if(dtopo_voies[idv3] == -1 || dtopo_voies[idv3] == dtopo_voies[idv2] + 1){

                                            // on stocke le parent (cas MULTIPLE)
                                            voie_parenteMLT[idv3] = idv2;

                                            // on fait les comptes (cas MULTIPLE)
                                            idv_filleMLT = idv3 ;
                                            idv_parentMLT =  voie_parenteMLT[idv_filleMLT]  ;
                                            if(idv_parentMLT != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parenteMLT[idv_filleMLT] != 0){
                                                voie_useMLT[idv_parentMLT] += 1;
                                                idv_filleMLT = idv_parentMLT;
                                                idv_parentMLT = voie_parenteMLT[idv_filleMLT];

                                                if(idv_parentMLT == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentMLT = %1)").arg(idv_parentMLT));
                                                    return false;
                                                }
                                            }

                                        }

                                        // = SI LA VOIE A DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] != -1){

                                            //on compare les longueurs d'accès
                                            if(lacces_voies[idv3] > length_voie[idv2] + lacces_voies[idv2]){
                                                //on a trouvé un chemin plus court en distance ! (même si équivalent plus grand en distance topologique)

                                                //pLogger->ATTENTION(QString("Chemin plus court en distance pour la voie %1 par rapport à la voie %2 : %3 (plus court que %4)").arg(idv3).arg(idv1).arg(length_voie[idv2] + lacces_voies[idv2]).arg(lacces_voies[idv3]));

                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT = voie_parenteLGT[idv_filleLGT];


                                                //si ce n'est pas la première fois
                                                if(idv_parentLGT != -1){

                                                    //pLogger->ATTENTION(QString("Ce n'est pas la première fois qu'un chemin plus court en distance est trouvé !"));


                                                    //IL FAUT ENLEVER L'INFO DANS USE AJOUTEE A TORD
                                                    //REMISE A NIVEAU DU USE
                                                    while(idv_parentLGT != 0){
                                                        voie_useLGT[idv_parentLGT] -= 1;
                                                        idv_filleLGT = idv_parentLGT;
                                                        idv_parentLGT = voie_parente[idv_filleLGT];

                                                        if(idv_parentLGT == -1){
                                                            pLogger->ERREUR(QString("REMISE A NIVEAU DU USE : Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                            return false;
                                                        }
                                                    }

                                                    //IL FAUT SUPPRIMER L'ANCIEN CHEMIN AJOUTE A TORD
                                                    //REMONTEE JUSQUA L'ANCETRE COMMUN

                                                 }

                                                //IL FAUT STOCKER LE NOUVEAU

                                                // on stocke le parent (cas DISTANCE MIN)
                                                voie_parenteLGT[idv3] = idv2;

                                                // on fait les comptes (cas DISTANCE MIN)
                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT =  voie_parenteLGT[idv_filleLGT]  ;

                                                while(idv_parentLGT != 0){
                                                    voie_useLGT[idv_parentLGT] += 1;
                                                    idv_filleLGT = idv_parentLGT;
                                                    idv_parentLGT = voie_parente[idv_filleLGT];

                                                    if(idv_parentLGT == -1){
                                                       pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                       return false;
                                                    }
                                                }


                                                /*else{ pLogger->ERREUR(QString("idv_parentLGT = %1)").arg(idv_parentLGT));
                                                      return false;
                                                };*/

                                            }// end if nouveau chemin plus court en distance

                                        }//end if déjà traitée


                                        // = SI LA VOIE N'A PAS DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] == -1){

                                            dtopo_voies[idv3] = dtopo +1;
                                            lacces_voies[idv3] = length_voie[idv2] + lacces_voies[idv2];
                                            nb_voiestraitees += 1;


                                            // on stocke le parent UNIQUE
                                            voie_parente[idv3] = idv2;

                                            // on fait les comptes UNIQUE
                                            idv_fille = idv3 ;
                                            idv_parent =  voie_parente[idv_fille]  ;
                                            if(idv_parent != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parente[idv_fille] != 0){
                                                voie_use[idv_parent] += 1;
                                                idv_fille = idv_parent;
                                                idv_parent = voie_parente[idv_fille];

                                                if(idv_parent == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parent = %1)").arg(idv_parent));
                                                    return false;
                                                }
                                            }

                                            /*//USE LGT = USE TOPO
                                            for(int v = 0; v < m_nbVoies + 1; v++){
                                                voie_parenteLGT[v] = voie_parente[v];
                                                voie_useLGT[v] = voie_use[v];
                                            }//end for v*/

                                        }//end if (voie non traitee)

                                     }//end if

                                }//end for v2 (idv3)

                            }//end if (on trouve les voies de l'ordre souhaite)

                        }//end if

                    }//end for idv2 : voie j

                    if(nb_voiestraitees == nb_voiestraitees_test && nb_voiestraitees != nbVOIESconnexes){
                        pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2 pour idv %3").arg(nb_voiestraitees).arg(nbVOIESconnexes).arg(idv1));
                        return false;
                    }//end if

                    dtopo += 1;

                }//end while (voies a traitees)

            }//end if

        }//end for idv1

        //CALCUL DE USE
        for(int idv = 1; idv < m_nbVoies + 1; idv++){

            int use_v = voie_use[idv];
            int useMLT_v = voie_useMLT[idv];

            int useLGT_v = voie_useLGT[idv];
            if(voie_useLGT[idv] == 0){useLGT_v = voie_use[idv];}

            QSqlQuery addUseAttInVOIES;
            addUseAttInVOIES.prepare("UPDATE VOIES SET USE = :USE, USE_MLT = :USE_MLT, USE_LGT = :USE_LGT WHERE idv = :IDV ;");
            addUseAttInVOIES.bindValue(":IDV", idv );
            addUseAttInVOIES.bindValue(":USE",use_v);
            addUseAttInVOIES.bindValue(":USE_MLT",useMLT_v);
            addUseAttInVOIES.bindValue(":USE_LGT",useLGT_v);

            if (! addUseAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'a structuralite'attribut use %1 pour la voie %2").arg(use_v).arg(idv));
                pLogger->ERREUR(addUseAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv

        if (! pDatabase->add_att_cl("VOIES", "CL_USE", "USE", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_USEMLT", "USE_MLT", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_USELGT", "USE_LGT", 10, true)) return false;

        pLogger->INFO("------------------------ calcUse END ----------------------");

    } else {
        pLogger->INFO("---------------- Use attributes already in VOIES -----------------");
    }

    return true;

}//END calcUse

bool Voies::calcInclusion(){

    if (! pDatabase->columnExists("VOIES", "INCL") || ! pDatabase->columnExists("VOIES", "INCL_MOY")) {
        pLogger->INFO("---------------------- calcInclusion START ----------------------");

        // AJOUT DE L'ATTRIBUT D'INCLUSION
        QSqlQueryModel addInclInVOIES;
        addInclInVOIES.setQuery("ALTER TABLE VOIES ADD INCL float, ADD INCL_MOY float;");

        if (addInclInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs d'inclusion dans VOIES : %1").arg(addInclInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromVOIES = new QSqlQueryModel();

        structFromVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM VOIES;");

        if (structFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans VOIES : %1").arg(structFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromVOIES->record(v).value("STRUCT_VOIE").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete structFromVOIES;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++){

            float inclusion = 0;
            float inclusion_moy = 0;

            //on cherche toutes les voies connectées
            for (int v = 0; v < m_VoieVoies.at(idv1).size(); v ++) {
                long idv2 = m_VoieVoies.at(idv1).at(v);

                inclusion += struct_voie[idv2];
            }//end for idv2


            if(m_VoieVoies.at(idv1).size() != 0) {inclusion_moy = inclusion / m_VoieVoies.at(idv1).size();}
            else {inclusion_moy = inclusion;}

            //INSERTION EN BASE
            QSqlQuery addInclAttInVOIES;
            addInclAttInVOIES.prepare("UPDATE VOIES SET INCL = :INCL, INCL_MOY = :INCL_MOY WHERE idv = :IDV ;");
            addInclAttInVOIES.bindValue(":IDV",idv1 );
            addInclAttInVOIES.bindValue(":INCL",inclusion);
            addInclAttInVOIES.bindValue(":INCL_MOY",inclusion_moy);

            if (! addInclAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'inclusion %1 et l'inclusion moyenne %2 pour la voie %3").arg(inclusion).arg(inclusion_moy).arg(idv1));
                pLogger->ERREUR(addInclAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv1

        if (! pDatabase->add_att_cl("VOIES", "CL_INCL", "INCL", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_INCLMOY", "INCL_MOY", 10, true)) return false;
        if (! pDatabase->add_att_difABS("VOIES", "DIFF_STIN", "CL_S", "CL_INCLMOY")) return false;

        pLogger->INFO(QString("STRUCTURALITE TOTALE SUR LE RESEAU : %1").arg(m_struct_tot));

    } else {
        pLogger->INFO("---------------- Incl attributes already in VOIES -----------------");
    }

    return true;

}//END calcInclusion





//***************************************************************************************************************************************************
//INSERT NV ATT DE LA TABLE INFO EN BDD
//
//***************************************************************************************************************************************************

bool Voies::insertINFO(){

    pLogger->INFO("-------------------------- start insertINFO --------------------------");

    //STOCKAGE DES INFOS*******

    if (! pDatabase->tableExists("INFO")){
        pLogger->ERREUR("La table INFO doit etre presente au moment des voies");
        return false;
    }//endif

    // Test si existe methode + seuil

    QSqlQueryModel alreadyInTable;
    alreadyInTable.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));
    if (alreadyInTable.lastError().isValid()) {
        pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de cette methode et seuil : %1").arg(alreadyInTable.lastError().text()));
        return false;
    }

    bool isInTable = (alreadyInTable.rowCount() > 0);
    bool isActive = false;
    if (isInTable) isActive = alreadyInTable.record(0).value("active").toBool();

    if (isInTable && isActive) {
        pLogger->INFO(QString("Methode = %1 et seuil = %2 est deja dans la table INFO, et est actif").arg(m_methode).arg(m_seuil_angle));
        return true;
    } else if (isInTable && ! isActive) {
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuratiosn de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQuery activeUn;
        activeUn.prepare(QString("UPDATE INFO SET ACTIVE=TRUE WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));

        if (! activeUn.exec()) {
            pLogger->ERREUR(QString("Impossible d'activer la bonne configuration toutes les configuration de la table info : %1").arg(activeUn.lastError().text()));
            return false;
        }

    } else {
        // La configuration n'est pas dans la table INFO
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuration de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQueryModel *req_voies_avg = new QSqlQueryModel();
        req_voies_avg->setQuery(  "SELECT "

                                  "AVG(nba) as AVG_NBA, "
                                  "AVG(nbs) as AVG_NBS, "
                                  "AVG(nbc) as AVG_NBC, "
                                  "AVG(rtopo) as AVG_O, "
                                  "AVG(struct) as AVG_S, "

                                  "STDDEV(length) as STD_L, "
                                  "STDDEV(nba) as STD_NBA, "
                                  "STDDEV(nbs) as STD_NBS, "
                                  "STDDEV(nbc) as STD_NBC, "
                                  "STDDEV(rtopo) as STD_O, "
                                  "STDDEV(struct) as STD_S, "

                                  "AVG(LOG(length)) as AVG_LOG_L, "
                                  "AVG(LOG(nba)) as AVG_LOG_NBA, "
                                  "AVG(LOG(nbs)) as AVG_LOG_NBS, "
                                  "AVG(LOG(nbc)) as AVG_LOG_NBC, "
                                  "AVG(LOG(rtopo)) as AVG_LOG_O, "
                                  "AVG(LOG(struct)) as AVG_LOG_S, "

                                  "STDDEV(LOG(length)) as STD_LOG_L, "
                                  "STDDEV(LOG(nba)) as STD_LOG_NBA, "
                                  "STDDEV(LOG(nbs)) as STD_LOG_NBS, "
                                  "STDDEV(LOG(nbc)) as STD_LOG_NBC, "
                                  "STDDEV(LOG(rtopo)) as STD_LOG_O, "
                                  "STDDEV(LOG(struct)) as STD_LOG_S "

                                  "FROM VOIES "

                                  "WHERE length > 0 AND nba > 0 AND nbs > 0 AND nbc > 0 AND rtopo > 0 AND struct > 0;");

        if (req_voies_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_voies_avg : %1").arg(req_voies_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_nba = req_voies_avg->record(0).value("AVG_NBA").toFloat();
        float avg_nbs = req_voies_avg->record(0).value("AVG_NBS").toFloat();
        float avg_nbc = req_voies_avg->record(0).value("AVG_NBC").toFloat();
        float avg_o = req_voies_avg->record(0).value("AVG_O").toFloat();
        float avg_s = req_voies_avg->record(0).value("AVG_S").toFloat();

        float std_length = req_voies_avg->record(0).value("STD_L").toFloat();
        float std_nba = req_voies_avg->record(0).value("STD_NBA").toFloat();
        float std_nbs = req_voies_avg->record(0).value("STD_NBS").toFloat();
        float std_nbc = req_voies_avg->record(0).value("STD_NBC").toFloat();
        float std_o = req_voies_avg->record(0).value("STD_O").toFloat();
        float std_s = req_voies_avg->record(0).value("STD_S").toFloat();

        float avg_log_length = req_voies_avg->record(0).value("AVG_LOG_L").toFloat();
        float avg_log_nba = req_voies_avg->record(0).value("AVG_LOG_NBA").toFloat();
        float avg_log_nbs = req_voies_avg->record(0).value("AVG_LOG_NBS").toFloat();
        float avg_log_nbc = req_voies_avg->record(0).value("AVG_LOG_NBC").toFloat();
        float avg_log_o = req_voies_avg->record(0).value("AVG_LOG_O").toFloat();
        float avg_log_s = req_voies_avg->record(0).value("AVG_LOG_S").toFloat();

        float std_log_length = req_voies_avg->record(0).value("STD_LOG_L").toFloat();
        float std_log_nba = req_voies_avg->record(0).value("STD_LOG_NBA").toFloat();
        float std_log_nbs = req_voies_avg->record(0).value("STD_LOG_NBS").toFloat();
        float std_log_nbc = req_voies_avg->record(0).value("STD_LOG_NBC").toFloat();
        float std_log_o = req_voies_avg->record(0).value("STD_LOG_O").toFloat();
        float std_log_s = req_voies_avg->record(0).value("STD_LOG_S").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_voies_avg;

        QSqlQueryModel *req_angles_avg = new QSqlQueryModel();
        req_angles_avg->setQuery( "SELECT "
                                  "AVG(angle) as AVG_ANG, "
                                  "STDDEV(angle) as STD_ANG "

                                  "FROM ANGLES WHERE USED;");

        if (req_angles_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_angles_avg : %1").arg(req_angles_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_ang = req_angles_avg->record(0).value("AVG_ANG").toFloat();
        float std_ang = req_angles_avg->record(0).value("STD_ANG").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_angles_avg;

        pLogger->INFO(QString("m_length_tot : %1").arg(m_length_tot));

        QSqlQuery info_in_db;
        info_in_db.prepare("INSERT INTO INFO ("

                           "methode, seuil_angle, LTOT, N_COUPLES, N_CELIBATAIRES, NV, "
                           "AVG_LVOIE, STD_LVOIE, AVG_ANG, STD_ANG, AVG_NBA, STD_NBA, AVG_NBS, STD_NBS, AVG_NBC, STD_NBC, AVG_O, STD_O, AVG_S, STD_S, "
                           "AVG_LOG_LVOIE, STD_LOG_LVOIE, AVG_LOG_NBA, STD_LOG_NBA, AVG_LOG_NBS, STD_LOG_NBS, AVG_LOG_NBC, STD_LOG_NBC, AVG_LOG_O, STD_LOG_O, AVG_LOG_S, STD_LOG_S, "
                           "ACTIVE) "

                           "VALUES ("

                           ":M, :SA, :LTOT, :N_COUPLES, :N_CELIBATAIRES, :NV, "
                           ":AVG_LVOIE, :STD_LVOIE, :AVG_ANG, :STD_ANG, :AVG_NBA, :STD_NBA, :AVG_NBS, :STD_NBS, :AVG_NBC, :STD_NBC, :AVG_O, :STD_O, :AVG_S, :STD_S, "
                           ":AVG_LOG_LVOIE, :STD_LOG_LVOIE, :AVG_LOG_NBA, :STD_LOG_NBA, :AVG_LOG_NBS, :STD_LOG_NBS, :AVG_LOG_NBC, :STD_LOG_NBC, :AVG_LOG_O, :STD_LOG_O, :AVG_LOG_S, :STD_LOG_S, "
                           "TRUE);");

        info_in_db.bindValue(":M",m_methode);
        info_in_db.bindValue(":SA",m_seuil_angle);
        info_in_db.bindValue(":LTOT",QVariant(m_length_tot));
        info_in_db.bindValue(":N_COUPLES",QVariant(m_nbCouples));
        info_in_db.bindValue(":N_CELIBATAIRES",QVariant(m_nbCelibataire));
        info_in_db.bindValue(":NV",QVariant(m_nbVoies));

        info_in_db.bindValue(":AVG_LVOIE",QVariant(m_length_tot/m_nbVoies));

        info_in_db.bindValue(":AVG_ANG",avg_ang);
        info_in_db.bindValue(":STD_ANG",std_ang);

        info_in_db.bindValue(":AVG_NBA",avg_nba);
        info_in_db.bindValue(":AVG_NBS",avg_nbs);
        info_in_db.bindValue(":AVG_NBC",avg_nbc);
        info_in_db.bindValue(":AVG_O",avg_o);
        info_in_db.bindValue(":AVG_S",avg_s);

        info_in_db.bindValue(":STD_LVOIE",std_length);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBS",std_nbs);
        info_in_db.bindValue(":STD_NBC",std_nbc);
        info_in_db.bindValue(":STD_O",std_o);
        info_in_db.bindValue(":STD_S",std_s);

        info_in_db.bindValue(":AVG_LOG_LVOIE",avg_log_length);
        info_in_db.bindValue(":AVG_LOG_NBA",avg_log_nba);
        info_in_db.bindValue(":AVG_LOG_NBS",avg_log_nbs);
        info_in_db.bindValue(":AVG_LOG_NBC",avg_log_nbc);
        info_in_db.bindValue(":AVG_LOG_O",avg_log_o);
        info_in_db.bindValue(":AVG_LOG_S",avg_log_s);

        info_in_db.bindValue(":STD_LOG_LVOIE",std_log_length);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBS",std_log_nbs);
        info_in_db.bindValue(":STD_LOG_NBC",std_log_nbc);
        info_in_db.bindValue(":STD_LOG_O",std_log_o);
        info_in_db.bindValue(":STD_LOG_S",std_log_s);


        if (! info_in_db.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer les infos (sur VOIES) dans la table INFO : %1").arg(info_in_db.lastError().text()));
            return false;
        }

    }

    pLogger->INFO("--------------------------- end insertINFO ---------------------------");

    return true;

}//END insertINFO

//***************************************************************************************************************************************************

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE DES VOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Voies(int buffer){

    // Construction des couples d'arcs
    if (! buildCouples(buffer)) return false;

    // Construction des attributs membres des voies
    if (! buildVectors()) return false;

    // Construction de la table VOIES en BDD
    if (! build_VOIES()) {
        if (! pDatabase->dropTable("VOIES")) {
            pLogger->ERREUR("build_VOIES en erreur, ROLLBACK (drop VOIES) echoue");
        } else {
            pLogger->INFO("build_VOIES en erreur, ROLLBACK (drop VOIES) reussi");
        }
        return false;
    }

    return true;

}//end do_Voie

//***************************************************************************************************************************************************
//INSERTION DES ATTRIBUTS DES VOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Att_Voie(bool connexion, bool use, bool inclusion){

    // Calcul de la structuralite
    if (! calcStructuralite()) {
        if (! pDatabase->dropColumn("VOIES", "RTOPO")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "STRUCT")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "CL_S")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column CL_S) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column CL_S) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "SOL")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column SOL) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column SOL) reussi");
        }
        return false;
    }

    //calcul des angles de connexions (orthogonalité)
    if (connexion && ! calcConnexion()) {
        if (! pDatabase->dropColumn("VOIES", "NBCSIN")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column NBCSIN) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column NBCSIN) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "NBCSIN_P")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column NBCSIN_P) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column NBCSIN_P) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "C_ORTHO")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column C_ORTHO) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column C_ORTHO) reussi");
        }
        return false;

    }

    //calcul des utilisation (betweenness)
    if (use && ! calcUse()) {
        if (! pDatabase->dropColumn("VOIES", "USE")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column USE) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column USE) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "USE_MLT")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column USE_MLT) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column USE_MLT) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "USE_LGT")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column USE_LGT) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column USE_LGT) reussi");
        }
        return false;
    }

    // Calcul de l'inclusion
    if (inclusion && ! calcInclusion()) {
        if (! pDatabase->dropColumn("VOIES", "INCL")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "INCL_MOY")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) reussi");
        }
        return false;
    }

    //construction de la table VOIES en BDD
    if (! insertINFO()) return false;

    return true;

}//end do_Att_Voie
