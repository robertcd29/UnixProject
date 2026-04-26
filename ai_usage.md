Documentatie AI – Faza 1

Tool folosit

Claude (claude.ai) – Claude Sonnet



1. parse_condition

Promptul dat catre AI

Am un program C care gestioneaza rapoarte de infrastructura. Fiecare raport e o structura binara stocata intr-un fisier, cu urmatoarele campuri:


typedef struct {
int id;
char inspector[32];
float latitude;
float longitude;
char category[32];
int severity;
time_t timestamp;
char description[64];
} Report;


Am nevoie de o functie cu semnatura:


int parse_condition(const char *input, char *field, char *op, char *value);


Input-ul e un string de forma field:operator:value, de exemplu severity:>=:2 sau category:==:road. Functia trebuie sa separe string-ul in cele trei parti si sa returneze 1 daca a mers, 0 daca nu.

Ce a generat AI-ul

AI-ul a generat o functie care cauta primul : din string ca sa extraga numele campului, apoi cauta un al doilea : de acolo inainte ca sa extraga operatorul, si ce ramane dupa e valoarea. A folosit strncpy cu null terminator explicit pentru fiecare parte si a pus si niste verificari de lungime.

Ce am schimbat si de ce

Nu am schimbat nimic. Am citit functia, am inteles cum merge si am testat-o.

Ce am invatat

Am aflat ca strchr primeste un pointer si cauta caracterul de acolo inainte, nu de la inceput. Asa ca daca ii dai first_colon + 1, sare peste primul : si il gaseste pe al doilea. Am invatat si ca operatorii >= si == trebuie pusi in ghilimele cand ii dai ca argumente in terminal, altfel bash ii interpreteaza gresit inainte sa ajunga la program.



2. match_condition

Promptul dat catre AI

Folosind aceeasi structura Report, am nevoie de o functie:


int match_condition(Report *r, const char *field, const char *op, const char *value);


Campurile suportate sunt severity (int), category (string), inspector (string), timestamp (time_t). Operatorii suportati sunt ==, !=, <, <=, >, >=. Functia returneaza 1 daca raportul respecta conditia, 0 daca nu.

Ce a generat AI-ul

AI-ul a generat o functie care verifica mai intai ce camp e, apoi ce operator e, si face comparatia corespunzatoare. Pentru severity si timestamp converteste valoarea din string in numar cu atoi si atol, pentru category si inspector foloseste strcmp.

Ce am schimbat si de ce

AI-ul pusese si operatorii <, >, <=, >= pentru campurile de tip string, dar nu are cum sa functioneze corect asa — nu poti compara doua categorii ca "road" si "flooding" cu mai mic sau mai mare. I-am scos si am lasat doar == si != pentru string-uri.

Ce am invatat

Am invatat ca pentru time_t trebuie folosit atol in loc de atoi, fiindca pe sisteme de 64 de biti valoarea poate fi mai mare decat ce incape intr-un int normal.



Rezumat

Ambele functii au fost generate de AI dupa ce i-am descris structura de date si ce trebuie sa faca fiecare functie. Le-am citit linie cu linie, le-am testat, si singura modificare pe care am facut-o a fost sa scot operatorii de comparare (<, >) pentru campurile de tip string din match_condition. Functia filter_reports, care deschide fisierul, citeste rapoartele, apeleaza parse_condition si match_condition si afiseaza rezultatele.