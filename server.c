#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	20
#define BUFLEN 256

void error(char *msg) {
	char *mss = "-10 : Eroare la apel ";
	mss = strcat(mss, msg);
	perror(mss);
	exit(1);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen;
	char buffer[BUFLEN], *aux, msg[BUFLEN];
	char **nume, **prenume, **card, **pin, **pass, **sold;
	char user[15] = "";

	struct sockaddr_in serv_addr, cli_addr;
	int n, i, j, check, try = 0, nrClients = 0, unlock = 0;
	int *logged, *sockl;


	if (argc != 3) {
		fprintf(stderr,"Usage : %s port input_file\n", argv[0]);
		exit(1);
	}

	// citire date clienti din fisier
	FILE *fp = NULL;
	fp = fopen(argv[2], "r");
	if (fp == NULL) {
		error("fopen");
	}

	fscanf(fp, "%d", &nrClients);
	fgets(buffer, BUFLEN, fp);

	// alocare dinamica pentru salvarea datelor clientilor
	nume = (char**)malloc(sizeof(char*) * nrClients);
	prenume = (char**)malloc(sizeof(char*) * nrClients);
	card = (char**)malloc(sizeof(char*) * nrClients);
	pin = (char**)malloc(sizeof(char*) * nrClients);
	pass = (char**)malloc(sizeof(char*) * nrClients);
	sold = (char**)malloc(sizeof(char*) * nrClients);
	logged = (int*)malloc(sizeof(int) * nrClients);
	sockl = (int*)malloc(sizeof(int) * nrClients);

	for (i = 0; i < nrClients; i++) {
		if (fgets(buffer, BUFLEN, fp) == NULL) {
			error("fgets");
		}

		nume[i] = (char*)malloc(sizeof(char) * 12);
		prenume[i] = (char*)malloc(sizeof(char) * 12);
		card[i] = (char*)malloc(sizeof(char) * 6);
		pin[i] = (char*)malloc(sizeof(char) * 4);
		pass[i] = (char*)malloc(sizeof(char) * 16);
		sold[i] = (char*)malloc(sizeof(char) * 16);
		logged[i] = 0;
		sockl[i] = 0;

		strcpy(nume[i], strtok(buffer, " \n"));
		strcpy(prenume[i], strtok(NULL, " \n"));
		strcpy(card[i], strtok(NULL, " \n"));
		strcpy(pin[i], strtok(NULL, " \n"));
		strcpy(pass[i], strtok(NULL, " \n"));
		strcpy(sold[i], strtok(NULL, " \n"));
	}



	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;	// multime folosita temporar 
	int fdmax;		// valoare maxima file descriptor din multimea read_fds

	// golim multimea de descriptori de citire si multimea tmp_fds 
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	// deschidere socketi TCP si UDP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("socket");
	}

	int sockUDP = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockUDP == -1) {
		error("socket UDP");
	}

	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		error("bind");
	}

	if (bind(sockUDP, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		error("bind");
	}

	listen(sockfd, MAX_CLIENTS);

	// adaugam stdin (0) si socketii pentru TCP si UDP in multimea de citire
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	FD_SET(sockUDP, &read_fds);
	fdmax = (sockfd > sockUDP) ? sockfd : sockUDP;

	// main loop
	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			error("select");
		}
		
		// comanda de la tastatura
		if (FD_ISSET(0, &tmp_fds)) {
			fgets(buffer, BUFLEN-1, stdin);
			buffer[strlen(buffer) - 1] = '\0';
			if (strcmp(buffer, "quit") == 0) {
				close(sockfd);
				close(sockUDP);
				strcpy(msg, "ATM> Server is shutting down");
				for (i = 1; i <= fdmax; i++) {
					if ((i == sockfd) || (i == sockUDP)) {
						continue;
					}
					if (FD_ISSET(i, &read_fds)) {
						check = send(i, msg, BUFLEN, 0);
						if (check == -1) {
							error("send");
						}
						FD_CLR(i, &read_fds);
						close(i);
					}
				}
				FD_ZERO(&tmp_fds);
				FD_ZERO(&read_fds);
				break;
			}
		}

		// verificare primire date pe socketi
		for(i = 1; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// avem date pe socketul inactiv = o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, 
								(struct sockaddr *)&cli_addr, &clilen)) == -1) {
						error("accept");
					} else {
						// adaug noul socket la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
				} else if (i == sockUDP) {
					// citire de pe socketul UDP
					int servlen = sizeof(serv_addr);
					check = recvfrom(sockUDP, buffer, BUFLEN, 0, 
						(struct sockaddr*)&serv_addr, &servlen);
					if (check == -1) {
						error("recvfrom");
					}

					char *aux = strtok(buffer, " \n");

					if (!strcmp(aux, "unlock")) {
						aux = strtok(NULL, " \n");
						for (j = 0; j < nrClients; j++) {
							if (!strcmp(card[j], aux)) {
								break;
							}
						}
						// nu s-a gasit cardul in lista 
						if (j == nrClients) {
							strcpy(msg, "UNLOCK> -4 : Numar card inexistent");
						} else {
							// verifica daca respectivul card este blocat
							if (logged[j] != -1) {
								strcpy(msg, "UNLOCK> -6 : Operatie esuata");
							} else {
								strcpy(msg, "UNLOCK> Trimite parola secreta");
							}
						}
					} else {
						// verificare parola secreta
						for (j = 0; j < nrClients; j++) {
							if (!strcmp(card[j], aux)) {
								break;
							}
						}

						if (j == nrClients) {
							strcpy(msg, "UNLOCK> -4 : Numar card inexistent");
						} else {
							aux = strtok(NULL, " \n");
							if (!strcmp(pass[j], aux)) {
								logged[j] = 0;
								strcpy(msg, "UNLOCK> Client deblocat");
							} else {
								strcpy(msg, "UNLOCK> -7 : Deblocare esuata");
							}
						}
					}

					check = sendto(sockUDP, msg, BUFLEN, 0, 
							(struct sockaddr*)&serv_addr,sizeof(serv_addr));
					if (check == -1) {
						error("sendto");
					}

				} else {
					// am primit date de la unul din clienti 
					// actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							//conexiunea s-a inchis; stergere client i din lista
							FD_CLR(i, &read_fds);
							close(i);
						} else {
							error("recv");
						}
					} 
					else { // recv intoarce >0
						printf("%s\n", buffer);

						strcpy(msg, "ATM> ");
						aux = strtok(buffer, " \n");

						if (strcmp(aux, "login") == 0) {
							aux = strtok(NULL, " \n");

							// cautare nr card in lista
							for (j = 0; j < nrClients; j++) {
								if (strcmp(card[j], aux) == 0) {
									// cardul este blocat
									if (logged[j] == -1) {
										strcat(msg, "-5 : Card blocat");
										break;
									}

									// verificare pin
									aux = strtok(NULL, " \n");
									if (strcmp(pin[j], aux) == 0) {
										if (logged[j] == 1) {
											strcat(msg, "-2 : Sesiune deja deschisa");
											break;
										}
										logged[j] = 1;
										sockl[j] = i;
										strcat(msg, "Welcome ");
										strcat(msg, nume[j]);
										strcat(msg, " ");
										strcat(msg, prenume[j]);
									} else {
										// s-a introdus pin gresit
										// se salveaza userul curent si nr de incercari
										if (strcmp(card[j], user) != 0) {
											strcpy(user, card[j]);
											try = 0;
										}
										if (++try == 3) {
											logged[j] = -1;
											strcat(msg, "-5 : Card blocat");
											try = 0;
										} else {
											strcat(msg, "-3 : Pin gresit");
											printf("Trials: %d\n", try);
										}							
									}
									break;
								}
							}
							if (strlen(msg) < 7) {
								strcat(msg, "-4 : Numar card inexistent");
							}
						} else {
							// pentru alta comanda in afara de login
							for (j = 0; j < nrClients; j++) {
								if (sockl[j] == i) {
									break;
								}
							}

							if (strcmp(aux, "logout") == 0) {
								strcat(msg, "Deconectare de la bancomat");
								logged[j] = 0;
								sockl[j] = 0;
							} else if (strcmp(aux, "listsold") == 0) {
								strcat(msg, sold[j]);
							} else if (strcmp(aux, "putmoney") == 0) {
								aux = strtok(NULL, " \n");
								sprintf(sold[j], "%.2f", atof(sold[j]) + atof(aux));
								strcat(msg, "Suma depusa cu succes");
							} else if (strcmp(aux, "getmoney") == 0) {
								aux = strtok(NULL, " \n");
								int s = atoi(aux);
								if ((s % 10) != 0) {
									strcat(msg, "-9 : Suma nu este multiplu de 10");
								} else {
									if (s > atof(sold[j])) {
										strcat(msg, "-8 : Fonduri insuficiente");
									} else {
										sprintf(sold[j], "%.2f", atof(sold[j]) - (float)s);
										sprintf(msg, "ATM> Suma %d retrasa cu succes", s);
									}
								}
							} else if (!strcmp(aux, "quit")) {
								FD_CLR(i, &read_fds);
								if (j != nrClients) {
									logged[j] = 0;
									sockl[j] = 0;
								}
							} else {
								// comanda incorecta
								strcat(msg, "-6 : Operatie esuata");
							}
						} 
						
						// trimitere mesaj
						check = send(i, msg, BUFLEN, 0);
						if (check == -1) {
							error("send");
						}
					}
				} 
			}
		}
	}

	close(sockfd);
	close(sockUDP);

	// eliberare memorie
	free(logged);
	free(sockl);
	for (i = 0; i < nrClients; i++) {
		free(nume[i]);
		free(prenume[i]);
		free(card[i]);
		free(pin[i]);
		free(sold[i]);
		free(pass[i]);
	}
   
	return 0; 
}
