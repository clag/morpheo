#ifndef SOMMET_H
#define SOMMET_H

#include "Graphe.h"
#include "Voies.h"

class Sommet
{
public:
    Sommet(Graphe* graphe, Voies* v);

    //calcul des attributs de sommet
    void calc_att_sommets(int NS, int NA, int NV, int seuil_angle, int methode, bool done_graphe, bool done_voies);

    //---construction des attributs de sommets
    void do_Att_Sommet(int NS, int NA, int NV, int NC, int seuil_angle, int methode, bool done_graphe, bool done_voies);

private:
    //graphe auquel les voies sont rattachées
    Graphe* m_Graphe;
    //voies auxquelles les sommets sont rattachés
    Voies* m_Voies;

};

#endif // SOMMET_H
