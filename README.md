Morpheo 0.8

Cet outil crée un graphe et reconstitue les voies, à partir de deux tables (base POSTGIS) brutes, afin de permettre le calcul de plusieurs indicateurs.

Tables en entrée :
- brut_arcs : LINESTRING, réseau routier, de manière à avoir un seul tronçon entre deux intersections (brut_sommets)
- brut_sommets : POINT, intersections du réseau routier

Tables propres au graphe en sortie :
- SXYZ
- SIF
- PAXYZ
- AXYZ
- ANGLES
- IMP (impasses)

Tables supplémentaires en sortie :
- VOIES
- INFO

Indicateurs :
- Connectivité
- Structuralité
- Maillance
- Espacement
