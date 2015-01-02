#include "Sommet.h"

#include <iostream>
using namespace std;

Sommet::Sommet(Graphe graphe, voie v)
{
    this->m_Graphe = graphe;
    this->m_Voies = v;
}

//***************************************************************************************************************************************************
//CALCUL DES ATTRIBUTS DE CENTRALITE D'UN SOMMET
//
//***************************************************************************************************************************************************

void Sommet::calc_att_sommets(int NS, int NA, int NV, int seuil_angle, int methode, bool done_graphe, bool done_voies){

    //if(!done_voies){
        //construction des tables liées aux voies
        m_Voies.build_couples(NS, NA, seuil_angle, methode, done_graphe);
        m_Voies.build_VoieArcs();
        m_Voies.build_SomVoies(NS);
        m_Voies.build_VoieSommets(NA);
    //}
    //else{
    //    m_Voies.m_SomVoies;
    //}

   cout<<endl<<endl<<"---------------------------calcul des attributs des sommets---------------------------"<<endl<<endl;

    //AJOUT ATTRIBUT DANS SXYZ : MOYENNE DES CLASSES DE STRUCTURALITE DES VOIES ****************************************************** CLS

    QSqlQueryModel addatt1_sxyz;
    addatt1_sxyz.setQuery("ALTER TABLE SXYZ ADD CLS float, ADD AVG_CLS float;");

    if (addatt1_sxyz.lastError().isValid()) {
        cout<<endl<< "ERREUR Report - addatt1_sxyz : "<< addatt1_sxyz.lastError().text().toStdString() << endl;
    }//end if : test requête QSqlQueryModel


    for(int s=0; s<NS; s++){

        float cls_som = 0;
        float avg_cls_som = 0;

        for(uint v=0; v<m_Voies.m_SomVoies[s].size(); v++){

            int idv = m_Voies.m_SomVoies[s][v];
            //cout<<"idv : "<<idv<<endl;

            QSqlQuery req_voie_cls;
            req_voie_cls.prepare("SELECT CL_S FROM VOIES WHERE IDV = :IDV;");
            req_voie_cls.bindValue(":IDV", idv);
            req_voie_cls.exec();

            QSqlQueryModel req_voie_cls_model;
            req_voie_cls_model.setQuery(req_voie_cls);

            if (req_voie_cls.lastError().isValid()) {
                cout<<endl<< "ERREUR Report - req_voie_cls : "<< req_voie_cls.lastError().text().toStdString() << endl;
            }//end if : test requête QSqlQueryModel

            int cls = req_voie_cls_model.record(0).value("CL_S").toInt();
            //cout<<"cls : "<<cls<<endl;

            cls_som += cls;

        }//end for v


        //moyenne sur le nombre de voies
        avg_cls_som = cls_som / m_Voies.m_SomVoies[s].size();

        //INSERTION EN BASE
        QSqlQuery req_avgcls_som_indb;
        req_avgcls_som_indb.prepare("UPDATE SXYZ SET CLS = :CLS, AVG_CLS = :AVG_CLS WHERE ids = :IDS ;");
        req_avgcls_som_indb.bindValue(":CLS", cls_som);
        req_avgcls_som_indb.bindValue(":AVG_CLS", avg_cls_som);
        req_avgcls_som_indb.bindValue(":IDS",s+1);

        if (! req_avgcls_som_indb.exec()) {
            cout<<endl<<" ERREUR : impossible d'inserer avg_cls pour le sommet " << s+1 << endl;
            cout<<endl << req_avgcls_som_indb.lastError().text().toStdString() << endl;
        }


    }//end for s


    //***************************************************************** SIMPLIEST SUR LES SOMMETS

    //AJOUT ATTRIBUT DANS SXYZ

    QSqlQueryModel addatt2_sxyz;
    addatt2_sxyz.setQuery("ALTER TABLE SXYZ ADD STRUCT_S float;");

    if (addatt2_sxyz.lastError().isValid()) {
        cout<<endl<< "ERREUR Report - addatt2_sxyz : "<< addatt2_sxyz.lastError().text().toStdString() << endl;
    }//end if : test requête QSqlQueryModel


    for(int s=0; s<NS; s++){


        //INITIALISATION
        //int centralite_s = 0;
        int ordretot_s = 0;
        int ordre = 0;
        //int simpliest_parties_non_connexe =0;
        int ordre_parties_non_connexe = 0;
        int nb_sommetstraites = 0;
        int nb_sommetstraites_test = 0;
        vector<int> V_ordreNombre;
        //vector<int> V_ordreLength;
        int ordre_sommet[NS];

        for(int i=0; i<NS; i++){
            ordre_sommet[i]=-1;
        }//end for i

        ordre_sommet[s]=ordre;
        nb_sommetstraites = 1;

        V_ordreNombre.push_back(1);
        //V_ordreLength.push_back(length_voie[v]);

        //-------------------

        //TRAITEMENT
        while(nb_sommetstraites != NS){

            //cout<<endl<<"nb_voiestraitees : "<<nb_voiestraitees<<" / "<<m_nbVoies<<" voies."<<endl;
            nb_sommetstraites_test = nb_sommetstraites;

            //TRAITEMENT DE LA LIGNE ORDRE+1 DANS LES VECTEURS
            V_ordreNombre.push_back(0);
            //V_ordreLength.push_back(0);

            //------------------------------------------sommet j
            for(int j=0; j<NS; j++){
                //on cherche toutes les sommets de l'ordre auquel on se trouve
                if(ordre_sommet[j]==ordre){

                    /*cout<<"SOMMET J : "<<j<<endl;
                    cout<<"LENGTH : "<<length_voie[j]<<endl;
                    cout<<"ORDRE : "<<ordre_voie[j]<<endl;*/

                    //on parcours les voies passant par le sommet
                    for(uint v=0; v<m_Voies.m_SomVoies[j].size(); v++){

                        long voie_somj = m_Voies.m_SomVoies[j][v];
                        //cout<<endl<<"som_voiej : "<<som_voiej<<endl;

                        //on cherche les sommets sur cette voie
                        for(uint s2=0; s2<m_Voies.m_VoieSommets[voie_somj].size(); s2++){

                            long som_somj = m_Voies.m_VoieSommets[voie_somj][s2];
                            //cout<<"voie_voiej : "<<voie_voiej<<endl;

                            //si le sommet n'a pas déjà été traitée
                            if(ordre_sommet[som_somj] == -1){

                                ordre_sommet[som_somj] = ordre +1;
                                nb_sommetstraites += 1;
                                V_ordreNombre.at(ordre+1) += 1;
                                //V_ordreLength.at(ordre+1) += length_voie[voie_voiej];

                            }//end if (sommet non traitée)

                        }//end for s2 (sommets sur la voie passant par le sommet)

                    }//end for v (voie passant par le sommet)

                }//end if (on trouve les sommets de l'ordre souhaité)

            }//end for j (sommet)

            //si aucun sommet n'a été trouvée comme connectée à celui qui nous intéresse
            if(nb_sommetstraites == nb_sommetstraites_test){
                //cout<<"Traitement de la partie non connexe : "<<m_nbVoies-nb_voiestraitees<<" voies traitees / "<<m_nbVoies<<" voies."<<endl;

                //for(int i=0; i<m_nbVoies; i++){
                    //if(ordre_voie[i]==-1){

                        int nbsommets_connexes = 0;
                        for(int k=0; k<NS; k++){
                            if(ordre_sommet[k]!=-1){
                                nbsommets_connexes += 1;
                            }
                            else{
                                nb_sommetstraites += 1;
                            }//end if
                        }//end for k

                        /*float length_connexe = 0;
                        for(uint l=0; l<V_ordreLength.size(); l++){
                            length_connexe += V_ordreLength.at(l);
                        }//end for l*/


                        //ordre_voie[i] = (m_nbVoies-nbvoies_connexe) * (length_tot-length_connexe);
                        //nb_voiestraitees += 1;
                        //V_ordreNombre.at(ordre+1) += 1;
                        //V_ordreLength.at(ordre+1) += length_voie[i];

                        //simpliest_parties_non_connexe = (m_nbVoies-nbvoies_connexe) * (length_tot-length_connexe);
                        ordre_parties_non_connexe = (NS-nbsommets_connexes);


                    //}//end if
                //}//end for i

                break;
            }// end if

            if(nb_sommetstraites ==nb_sommetstraites_test){
                cout<<"ERREUR : seulement "<<nb_sommetstraites<<" sommets traitees / "<<NS<<" sommets."<<endl;
            }//end if

            ordre += 1;


        }//end while (sommets à traiter)

        //CALCUL DE L'ORDRE
        for(uint l=0; l<V_ordreNombre.size(); l++){

            ordretot_s += l * V_ordreNombre.at(l);

        }//end for l (calcul de l'ordre de la voie)

        ordretot_s += ordre_parties_non_connexe;

        cout<<endl<<"ORDRE : "<<ordretot_s<<endl;

        //INSERTION EN BASE
        QSqlQuery req_simpliest_som_indb;
        req_simpliest_som_indb.prepare("UPDATE SXYZ SET STRUCT_S = :STRUCT_S WHERE ids = :IDS ;");
        req_simpliest_som_indb.bindValue(":STRUCT_S", ordretot_s);
        req_simpliest_som_indb.bindValue(":IDS",s+1);

        if (! req_simpliest_som_indb.exec()) {
            cout<<endl<<" ERREUR : impossible d'inserer simpliest_som pour le sommet " << s+1 << endl;
            cout<<endl << req_simpliest_som_indb.lastError().text().toStdString() << endl;
        }

    }//end for s


    //*****************************************************************

    QSqlQueryModel *req_voies = new QSqlQueryModel();

    req_voies->setQuery(  "SELECT "
                          "length AS LENGTH_VOIE "
                          "FROM VOIES "
                          "ORDER BY idv ASC;");

    if (req_voies->lastError().isValid()) {
        cout<<endl<< "ERREUR Report - calc_simpliest - req_voies : "<< req_voies->lastError().text().toStdString() << endl;
    }//end if : test requête QSqlQueryModel

    if(req_voies->rowCount() != m_Voies.m_nbVoies){ cout<<"ERREUR : Le nombre de lignes ne correspond pas /!\\ NOMBRE DE VOIES "<<endl; exit(EXIT_FAILURE); }

    //CREATION DU TABLEAU DONNANT LA LONGUEUR DE CHAQUE VOIE
    float length_voie[m_Voies.m_nbVoies];
    float length_tot = 0;
    for(int v=0; v<NV; v++){
        length_voie[v]=req_voies->record(v).value("LENGTH_VOIE").toFloat();
        length_tot += length_voie[v];
    }//end for v

    //SUPPRESSION DE L'OBJET
    delete req_voies;


    //AJOUT ATTRIBUT DANS SXYZ

    QSqlQueryModel addatt3_sxyz;
    addatt3_sxyz.setQuery("ALTER TABLE SXYZ ADD STRUCT_V float, ADD ORDRE_V integer;");

    if (addatt3_sxyz.lastError().isValid()) {
        cout<<endl<< "ERREUR Report - addatt3_sxyz : "<< addatt3_sxyz.lastError().text().toStdString() << endl;
    }//end if : test requête QSqlQueryModel

    //pour chaque sommet
    for(int s=0; s<NS; s++){


        //INITIALISATION
        int centralite_s = 0;
        int ordre_s = 0;
        int ordre_voie = 0;

        int simpliest_parties_non_connexe =0;
        int ordre_parties_non_connexe = 0;

        int nb_voiestraitees = 0;
        int nb_voiestraitees_test = 0;

        vector<int> V_ordreNombre;
        vector<int> V_ordreLength;
        int ordre_voie_tab[NV];

        for(int i=0; i<NV; i++){
            ordre_voie_tab[i]=-1;
        }//end for i

        V_ordreNombre.push_back(0);
        V_ordreLength.push_back(0);

        //on parcours toutes les voies
        for(int v=0; v<NV; v++){

            //on parcours tous les sommets sur cette voie
            for(uint s2=0; s2<m_Voies.m_VoieSommets[v].size(); s2++){

                //si on trouve notre sommet
                if(m_Voies.m_VoieSommets[v][s2]==s){

                    //on donne à la voie l'ordre 0
                    ordre_voie_tab[v]=ordre_voie;
                    //on incrémente le nombre de voies traitees
                    nb_voiestraitees++;
                    //on actualise le vecteur ordreNombre pour l'ordre 0
                    V_ordreNombre.at(ordre_voie) += 1;
                    V_ordreLength.at(ordre_voie) += length_voie[v];

                }//end if

            }//end fot s2
        }//end for v

        //-------------------

        //TRAITEMENT
        //si on n'a pas encore parcouru toutes les voies
        while(nb_voiestraitees != NV){

            //cout<<endl<<"nb_voiestraitees : "<<nb_voiestraitees<<" / "<<m_nbVoies<<" voies."<<endl;
            nb_voiestraitees_test = nb_voiestraitees;

            //TRAITEMENT DE LA LIGNE ORDRE+1 DANS LES VECTEURS
            V_ordreNombre.push_back(0);
            V_ordreLength.push_back(0);

            //------------------------------------------voie v
            for(int v=0; v<NV; v++){
                //on cherche toutes les voies de l'ordre auquel on se trouve
                if(ordre_voie_tab[v]==ordre_voie){

                    /*cout<<"VOIE J : "<<j<<endl;
                    cout<<"LENGTH : "<<length_voie[j]<<endl;
                    cout<<"ORDRE : "<<ordre_voie_tab[j]<<endl;*/

                    //on parcours les sommets sur la voie
                    for(uint s2=0; s2<m_Voies.m_VoieSommets[v].size(); s2++){

                        long som_voiev = m_Voies.m_VoieSommets[v][s2];
                        //cout<<endl<<"som_voiej : "<<som_voiej<<endl;

                        //on cherche les voies passant par ces sommets
                        for(uint v2=0; v2<m_Voies.m_SomVoies[som_voiev].size(); v2++){

                            long voie_voiev = m_Voies.m_SomVoies[som_voiev][v2];
                            //cout<<"voie_voiej : "<<voie_voiej<<endl;

                            //si la voie n'a pas déjà été traitée
                            if(ordre_voie_tab[voie_voiev] == -1){

                                ordre_voie_tab[voie_voiev] = ordre_voie +1;
                                nb_voiestraitees += 1;
                                V_ordreNombre.at(ordre_voie+1) += 1;
                                V_ordreLength.at(ordre_voie+1) += length_voie[voie_voiev];

                            }//end if (voie non traitée)

                        }//end for v2 (voies passant par les sommets sur la voie)

                    }//end for s2 (sommets sur la voie)

                }//end if (on trouve les voies de l'ordre souhaité)

            }//end for v (voie)

            //si aucune voie n'a été trouvée comme connectée à celle qui nous intéresse
            if(nb_voiestraitees == nb_voiestraitees_test){
                //cout<<"Traitement de la partie non connexe : "<<m_nbVoies-nb_voiestraitees<<" voies traitees / "<<m_nbVoies<<" voies."<<endl;

                //for(int i=0; i<m_nbVoies; i++){
                    //if(ordre_voie[i]==-1){

                        int nbvoies_connexe = 0;
                        for(int k=0; k<NV; k++){
                            if(ordre_voie_tab[k]!=-1){
                                nbvoies_connexe += 1;
                            }
                            else{
                                nb_voiestraitees += 1;
                            }//end if
                        }//end for k

                        float length_connexe = 0;
                        for(uint l=0; l<V_ordreLength.size(); l++){
                            length_connexe += V_ordreLength.at(l);
                        }//end for l


                        //ordre_voie[i] = (m_nbVoies-nbvoies_connexe) * (length_tot-length_connexe);
                        //nb_voiestraitees += 1;
                        //V_ordreNombre.at(ordre+1) += 1;
                        //V_ordreLength.at(ordre+1) += length_voie[i];

                        simpliest_parties_non_connexe = (NV-nbvoies_connexe) * (length_tot-length_connexe);
                        ordre_parties_non_connexe = (NV-nbvoies_connexe);


                    //}//end if
                //}//end for i

                break;
            }// end if

            if(nb_voiestraitees == nb_voiestraitees_test){
                cout<<"ERREUR : seulement "<<nb_voiestraitees<<" voies traitees / "<<NV<<" voies."<<endl;
            }//end if

            ordre_voie += 1;


        }//end while (voies à traitées)

        //CALCUL DE L'ORDRE
        for(uint l=0; l<V_ordreNombre.size(); l++){

            ordre_s += l * V_ordreNombre.at(l);

        }//end for l (calcul de l'ordre de la voie)

        ordre_s += ordre_parties_non_connexe;

        //cout<<endl<<"ORDRE : "<<ordre_s<<endl;

        //CALCUL DE LA CENTRALITE
        for(uint l=0; l<V_ordreLength.size(); l++){

            centralite_s += l * V_ordreLength.at(l);

        }//end for l (calcul de la simpliest centrality)

        centralite_s += simpliest_parties_non_connexe;

        //cout<<endl<<"SIMPLIEST : "<<centralite_s<<endl;

        //INSERTION EN BASE
        QSqlQuery req_simpliest_som_indb;
        req_simpliest_som_indb.prepare("UPDATE SXYZ SET STRUCT_V = :STRUCT_V, ORDRE_V = :ORDRE_V WHERE ids = :IDS ;");
        req_simpliest_som_indb.bindValue(":STRUCT_V", centralite_s);
        req_simpliest_som_indb.bindValue(":ORDRE_V", ordre_s);
        req_simpliest_som_indb.bindValue(":IDS",s+1);

        if (! req_simpliest_som_indb.exec()) {
            cout<<endl<<" ERREUR : impossible d'inserer simpliest_som pour le sommet " << s+1 << endl;
            cout<<endl << req_simpliest_som_indb.lastError().text().toStdString() << endl;
        }

    }//end for s

     cout<<endl<<endl<<"---------------------------FIN calcul des attributs des sommets---------------------------"<<endl<<endl;


}// END CALC ATT SOMMETS


//***************************************************************************************************************************************************
//INSERTION DES ATTRIBUTS DES SOMMETS
//
//***************************************************************************************************************************************************

void Sommet::do_Att_Sommet(int NS, int NA, int NV, int NC, int seuil_angle, int methode, bool done_graphe, bool done_voies){

    cout<<"m_Graphe.m_nbSommets : "<<NS<<endl;
    cout<<"m_Graphe.m_nbArcs : "<<   NA<<endl;
    cout<<"m_Voies.m_nbCouples : "<< NC<<endl;

    //CALCUL ET INSERTION DES ATTRIBUTS
    calc_att_sommets(NS, NA, NV, seuil_angle, methode, done_graphe, done_voies);

}
//end do_Att_Sommet



