#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

#define BUFLEN 256

void error(char *msg, FILE *fp) {
	char *mss = "-10 : Eroare la apel ";
	mss = strcat(mss, msg);
	printf("%s\n", mss);
	fputs(mss, fp);
	fclose(fp);
	perror(mss);
	exit(1);
}

int main(int argc, char *argv[]) {

	int sockfd, n, i, logged = 0, check, unlock = 0;
	struct sockaddr_in serv_addr;
	char user[15], buffer[BUFLEN];
	char filename[30];

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds; //multime folosita temporar 
	int fdmax;	  //valoare maxima file descriptor din multimea read_fds

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc != 3) {
	   fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
	   exit(0);
	}

	// deschidere fisier log
	sprintf(filename, "client-%d.log", getpid());
	FILE* fp = fopen(filename, "w");
	if (fp == NULL) {
		printf("Error opening file\n");
		return -1;
	}
	
	// obtinere socketi
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("socket", fp);
	}

	int sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockUDP == -1) {
		error("socket UDP", fp);
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &serv_addr.sin_addr);
	
	// conectare la server
	if (connect(sockfd, (struct sockaddr*)&serv_addr,
			sizeof(serv_addr)) < 0) {
		error("connect", fp);	
	}

	// setare socket 0 (stdin) si sockfd intors de apelul socket()
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	// main loop
	while(1){

		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			error("select", fp);
		}

		// citire de la tastatura, parsare si trimitere mesaj
		memset(buffer, 0, BUFLEN);
		if (FD_ISSET(0, &tmp_fds)) {
			fgets(buffer, BUFLEN-1, stdin);
			fputs(buffer, fp);
			buffer[strlen(buffer) - 1] = '\0';

			// verificare comanda citita
			if (strstr(buffer, "login")) {
				if (logged) {
					printf("-2 : Sesiune deja deschisa\n");
					fputs("-2 : Sesiune deja deschisa\n", fp);
				} else {
					// salvare nr card pentru comanda unlock
					strcpy(user, buffer+6);
					user[6] = '\0';

					check = send(sockfd, buffer, BUFLEN, 0);
					if (check == -1) {
						error("send", fp);
					}
				}
			} else if (strcmp(buffer, "logout") == 0) {
				if (logged) {
					logged = 0;
					check = send(sockfd, buffer, BUFLEN, 0);
					if (check == -1) {
						error("send", fp);
					}
				} else {
					printf("-1 : Clientul nu este autentificat\n");
					fputs("-1 : Clientul nu este autentificat\n", fp);
				}
			} else if (!strcmp(buffer, "unlock")) {
				// trimitere mesaj de unlock pe socket UDP
				unlock = 1;
				strcat(buffer, " ");
				strcat(buffer, user);
				check = sendto(sockUDP, buffer, BUFLEN, 0, 
					(struct sockaddr*)&serv_addr,sizeof(serv_addr));
				if (check == -1) {
					error("sendto", fp);
				}

				// setare socket UDP pentru multiplexare
				FD_SET(sockUDP, &read_fds);
				fdmax = (fdmax > sockUDP) ? fdmax : sockUDP;
			} else if (unlock) {
				// a doua comanda dupa unlock ar trebui sa fie parola secreta
				char aux[16];
				strcpy(aux, buffer);
				sprintf(buffer, "%s %s", user, aux);
				unlock = 0;
				check = sendto(sockUDP, buffer, BUFLEN, 0, 
					(struct sockaddr*)&serv_addr,sizeof(serv_addr));
				if (check == -1) {
					error("sendto", fp);
				}
			} else if (strstr(buffer, "quit")) {
				check = send(sockfd, buffer, BUFLEN, 0);
				if (check == -1) {
					error("send", fp);
				}
				break;
			} else {
				if (logged) {
					check = send(sockfd, buffer, BUFLEN, 0);
					if (check == -1) {
						error("send", fp);
					}
				} else {
					// orice comanda in afara de login este ignorata
					printf("-1 : Clientul nu este autentificat\n");
					fputs("-1 : Clientul nu este autentificat\n", fp);
				}
			}

		}

		// citire de pe socket UDP
		if (FD_ISSET(sockUDP, &tmp_fds)) {
			int servlen = sizeof(serv_addr);
			check = recvfrom(sockUDP, buffer, BUFLEN, 0, 
				(struct sockaddr*)&serv_addr, &servlen);
			if (check == -1) {
				error("recvfrom", fp);
			}
			printf ("%s\n", buffer);
			fputs(buffer, fp);
			fputs("\n", fp);
		}

		// citire de pe socket TCP
		if (FD_ISSET(sockfd, &tmp_fds)) {
			if ((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0) {
				if (n == 0) {
					//conexiunea s-a inchis
					printf("Bancomatul s-a deconectat\n");
					fputs("Bancomatul s-a deconectat\n", fp);
					FD_CLR(sockfd, &read_fds);
					break;
				} else {
					close(sockfd);
					close(sockUDP);
					error("recv", fp);
				}
			} 
			else { //recv intoarce > 0
				// daca login s-a efectuat cu succes
				if (strstr(buffer, "Welcome")) {
					logged = 1;
				}
				printf ("%s\n", buffer);
				fputs(buffer, fp);
				fputs("\n", fp);
			}
		}
	}

	fclose(fp);
	close(sockfd);
	close(sockUDP);

	return 0;
}
