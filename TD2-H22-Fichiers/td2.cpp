﻿#pragma region "Includes"//{
#define _CRT_SECURE_NO_WARNINGS // On permet d'utiliser les fonctions de copies de chaînes qui sont considérées non sécuritaires.

#include "structures.hpp"      // Structures de données pour la collection de films en mémoire.

#include "bibliotheque_cours.hpp"
#include "verification_allocation.hpp" // Nos fonctions pour le rapport de fuites de mémoire.

#include <iostream>
#include <fstream>
#include <sstream>

#include <memory>

#include <string>
#include <limits>
#include <algorithm>
#include "cppitertools/range.hpp"
#include "gsl/span"
#include <functional>

#include "debogage_memoire.hpp"        // Ajout des numéros de ligne des "new" dans le rapport de fuites.  Doit être après les include du système, qui peuvent utiliser des "placement new" (non supporté par notre ajout de numéros de lignes).
using namespace std;
using namespace iter;
using namespace gsl;

#pragma endregion//}

typedef uint8_t UInt8;
typedef uint16_t UInt16;

#pragma region "Fonctions de base pour lire le fichier binaire"//{

UInt8 lireUint8(istream& fichier)
{
    UInt8 valeur = 0;
    fichier.read((char*)&valeur, sizeof(valeur));
    return valeur;
}
UInt16 lireUint16(istream& fichier)
{
    UInt16 valeur = 0;
    fichier.read((char*)&valeur, sizeof(valeur));
    return valeur;
}
string lireString(istream& fichier)
{
    string texte;
    texte.resize(lireUint16(fichier));
    fichier.read((char*)&texte[0], streamsize(sizeof(texte[0])) * texte.length());
    return texte;
}

#pragma endregion//}

//TODO: Une fonction pour ajouter un Film à une ListeFilms, le film existant déjà; on veut uniquement ajouter le pointeur vers le film existant.  Cette fonction doit doubler la taille du tableau alloué, avec au minimum un élément, dans le cas où la capacité est insuffisante pour ajouter l'élément.  Il faut alors allouer un nouveau tableau plus grand, copier ce qu'il y avait dans l'ancien, et éliminer l'ancien trop petit.  Cette fonction ne doit copier aucun Film ni Acteur, elle doit copier uniquement des pointeurs.
void ListeFilms::ajouterFilm(Film* film)
{
    if (nElements_ == capacite_)
        changeDimension(max(1, capacite_ * 2));
    elements_[nElements_++] = film;
}

void ListeFilms::changeDimension(int nouvelleCapacite)
{
    Film** nouvelleListe = new Film * [nouvelleCapacite];

    if (elements_ != nullptr) 
    {
        nElements_ = min(nouvelleCapacite, nElements_);

        for (int i : range(nElements_))
            nouvelleListe[i] = elements_[i];
        delete[] elements_;
    }

    elements_ = nouvelleListe;
    capacite_ = nouvelleCapacite;
}

//TODO: Une fonction pour enlever un Film d'une ListeFilms (enlever le pointeur) sans effacer le film; la fonction prenant en paramètre un pointeur vers le film à enlever.  L'ordre des films dans la liste n'a pas à être conservé.
void ListeFilms::enleverFilm(const Film* film)
{
    for (Film*& element : enSpan()) {  // Doit être une référence au pointeur pour pouvoir le modifier.
        if (element == film) {
            if (nElements_ > 1)
                element = elements_[nElements_ - 1];
            nElements_--;
            return;
        }
    }
}
span<Film*> ListeFilms::enSpan() const { return span(elements_, nElements_); }

//TODO: Une fonction pour trouver un Acteur par son nom dans une ListeFilms, qui retourne un pointeur vers l'acteur, ou nullptr si l'acteur n'est pas trouvé.  Devrait utiliser span.

span<shared_ptr<Acteur>> ListeActeurs::enSpan() const { return span(elements_.get(), nElements_); }
//NOTE: Doit retourner un Acteur modifiable, sinon on ne peut pas l'utiliser pour modifier l'acteur tel que demandé dans le main, et on ne veut pas faire écrire deux versions.
shared_ptr < Acteur > ListeFilms::trouverActeur(const string& nomActeur) const
{
    for (const Film* film : enSpan()) {
        for (shared_ptr < Acteur > acteur : film->acteurs.enSpan()) {
            if (acteur->nom == nomActeur)
                return acteur;
        }
    }
    return nullptr;
}

//TODO: Compléter les fonctions pour lire le fichier et créer/allouer une ListeFilms.  La ListeFilms devra être passée entre les fonctions, pour vérifier l'existence d'un Acteur avant de l'allouer à nouveau (cherché par nom en utilisant la fonction ci-dessus).
shared_ptr<Acteur> lireActeur(istream& fichier, ListeFilms& listeFilms)
{
    Acteur acteur = {};
    acteur.nom = lireString(fichier);
    acteur.anneeNaissance = lireUint16(fichier);
    acteur.sexe = lireUint8(fichier);
    
    shared_ptr<Acteur> acteurExistant(listeFilms.trouverActeur(acteur.nom));

    if (acteurExistant.get() != nullptr)
        return acteurExistant;
    else {
        std::cout << "Création Acteur " << acteur.nom << endl;
        shared_ptr<Acteur> nouveauActeur = make_shared<Acteur>(acteur);
        return nouveauActeur;
    }
    
    return {}; //TODO: Retourner un pointeur soit vers un acteur existant ou un nouvel acteur ayant les bonnes informations, selon si l'acteur existait déjà.  Pour fins de débogage, affichez les noms des acteurs crées; vous ne devriez pas voir le même nom d'acteur affiché deux fois pour la création.
}

Film* lireFilm(istream& fichier, ListeFilms& listeFilms)
{
    Film* film = new Film;
    film->titre = lireString(fichier);
    film->realisateur = lireString(fichier);
    film->anneeSortie = lireUint16(fichier);
    film->recette = lireUint16(fichier);
    film->acteurs.assignerNElements(lireUint8(fichier));  //NOTE: Vous avez le droit d'allouer d'un coup le tableau pour les acteurs, sans faire de réallocation comme pour ListeFilms.  Vous pouvez aussi copier-coller les fonctions d'allocation de ListeFilms ci-dessus dans des nouvelles fonctions et faire un remplacement de Film par Acteur, pour réutiliser cette réallocation.
    film->acteurs;
    std::cout << "Création Film " << film->titre << endl;

    for (shared_ptr < Acteur > acteur : film->acteurs.enSpan()) {
        acteur = make_shared<Acteur>(lireActeur(fichier, listeFilms));
    }

    return film;
}

ListeFilms::ListeFilms(const string& nomFichier) : possedeLesFilms_(true)
{
    ifstream fichier(nomFichier, ios::binary);
    fichier.exceptions(ios::failbit);

    int nElements = lireUint16(fichier);

    for ([[maybe_unused]] int i : range(nElements)) { //NOTE: On ne peut pas faire un span simple avec spanListeFilms car la liste est vide et on ajoute des éléments à mesure.
        ajouterFilm(lireFilm(fichier, *this)); //TODO: Ajouter le film à la liste.
    }
}

void detruireActeur(Acteur * acteur) {
        cout << "Destruction Acteur " << acteur->nom << endl;
        delete acteur;
}

bool joueEncore(const Acteur* acteur) {
    return acteur->joueDans.size() != 0;
}

//TODO: Une fonction pour détruire un film (relâcher toute la mémoire associée à ce film, et les acteurs qui ne jouent plus dans aucun films de la collection).  Noter qu'il faut enleve le film détruit des films dans lesquels jouent les acteurs.  Pour fins de débogage, affichez les noms des acteurs lors de leur destruction.
void detruireFilm(Film * film)
{ 
    for (shared_ptr<Acteur> acteur : film->acteurs.enSpan()) 
    {
        acteur.get()->joueDans.enleverFilm(film);
        if (!joueEncore(acteur.get()))
            detruireActeur(acteur.get());
    }

    cout << "Destruction Film " << film->titre << endl;
    delete film;
}

//TODO: Une fonction pour détruire une ListeFilms et tous les films qu'elle contient.
ListeFilms::~ListeFilms() {
    if (possedeLesFilms_)
        for (Film* film : enSpan())
            detruireFilm(film);
        delete[] elements_;
}

ostream& afficherActeur(ostream& flux, const Acteur & acteur) {
    return flux << "  " << acteur.nom << ", " << acteur.anneeNaissance << " " << acteur.sexe << endl;
}

//TODO: Une fonction pour afficher un film avec tous ces acteurs (en utilisant la fonction afficherActeur ci-dessus).
ostream& afficherFilm(ostream& flux, const Film& film) {
    flux << "Titre: " << film.titre << endl;
    flux << "  Réalisateur: " << film.realisateur << "  Année :" << film.anneeSortie << endl;
    flux << "  Recette: " << film.recette << "M$" << endl;
    flux << "Acteurs:" << endl;

    for (shared_ptr<Acteur> acteur : film.acteurs.enSpan())
        afficherActeur(flux, *acteur.get());

    return flux;
}

void afficherListeFilms(const ListeFilms & listeFilms)
{
    static const string ligneDeSeparation = "\033[32m────────────────────────────────────────\033[0m\n";
     
    cout << ligneDeSeparation;
        
    for (const Film* film : listeFilms.enSpan()) {
        std::cout << film;
        std::cout << ligneDeSeparation;
    }
}

ostream& operator<<(ostream& flux, const Film& film) {
    return afficherFilm(flux, film);
}

Film* ListeFilms::operator[](int indice) {
    return this->elements_[indice];
}

shared_ptr<Acteur> ListeActeurs::operator[](int indice){
    return this->elements_[indice];
}



int main()
{
    #ifdef VERIFICATION_ALLOCATION_INCLUS
    bibliotheque_cours::VerifierFuitesAllocations verifierFuitesAllocations;
    #endif
    bibliotheque_cours::activerCouleursAnsi();

    static const string ligneDeSeparation = "\n\033[35m════════════════════════════════════════\033[0m\n";

    ListeFilms listeFilms("films.bin");

    std::cout << ligneDeSeparation << "Le premier film de la liste est:" << endl;
    //TODO: Afficher le premier film de la liste.  Devrait être Alien.
    cout << listeFilms[0];

    cout << ligneDeSeparation << "Les films sont:" << endl;
    //TODO: Afficher la liste des films.  Il devrait y en avoir 7.
    afficherListeFilms(listeFilms);

    //Chapitre 7

    ostringstream tamponStringStream;
    tamponStringStream << listeFilms[0];
    string filmEnString = tamponStringStream.str();
    cout << filmEnString;


    // Chapitre 7-8
    Film skylien = *listeFilms[0];
    skylien.titre = "Skylien";
    skylien.acteurs[0] = listeFilms[1]->acteurs[0];
    skylien.acteurs[0]->nom = "Daniel Wroughton Craig";
    std::cout << ligneDeSeparation << "skylien" << endl;
    std::cout << &skylien << endl;
    std::cout << ligneDeSeparation << "listeFilms[0]" << endl;
    std::cout << listeFilms[0] << endl;
    std::cout << ligneDeSeparation << "listeFilms[1]" << endl;
    std::cout << listeFilms[1] << endl;

    return 0;
}