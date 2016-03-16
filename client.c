#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>

#define SERV_IP "192.168.0.0" // Original server IP replaced
#define UDP_SERV_PORT 31180
#define TCP_SERV_PORT 80
#define KWORD "tele3118project"
#define MAX_MSG_LEN 128
#define SURVEY_SIZE 5000

typedef struct {
   char keyword[16];
   char userid[16];
} RegMsg_t;

typedef struct {
  unsigned long errorcode;
  char passwd[16];
  char errStr[108];
} RespMsg_t;

void parseSurvey(char *survey, char ***surveyCont);
void parseRecord(char *record, char **recordCont);
double gaussianrvGen(int privacyLevel); // Removed as it was not my own

int main(int argc, char *argv[]) {
// UDP SECTION
   RegMsg_t *regMsg;
   int sock, sAddrLen = sizeof(struct sockaddr_in);
   struct sockaddr_in *servAddr, *recvAddr;
   RespMsg_t *servMsg;

   printf("\nPlease enter your user ID: ");
   regMsg = malloc(sizeof(RegMsg_t));
   strcpy(regMsg->keyword, KWORD);
   scanf("%s", regMsg->userid);

   // Initialise server address for UDP connection
   servAddr = malloc(sAddrLen);
   bzero(servAddr, sAddrLen);
   servAddr->sin_family = AF_INET;
   inet_pton(AF_INET, SERV_IP, &(servAddr->sin_addr.s_addr));
   servAddr->sin_port = htons(UDP_SERV_PORT);

   // UDP connection - user password
   sock = socket(AF_INET, SOCK_DGRAM, 0);
   sendto(sock, regMsg, sizeof(RegMsg_t), 0,
          (struct sockaddr *)servAddr, sAddrLen);
   recvAddr = malloc(sAddrLen);
   servMsg = malloc(sizeof(RespMsg_t));
   recvfrom(sock, servMsg, sizeof(RespMsg_t), 0,
            (struct sockaddr *)recvAddr, (socklen_t *)&sAddrLen);
   if (ntohl(servMsg->errorcode) == 0) {
      printf("Your password is %s\n", servMsg->passwd);
   }
   close(sock);

// TCP SECTION
   char *command, *survey, ***surveyCont, *statResp, *record, **recordCont,
        *input, *s, *r;
   int bytes, numStaff, privacyLevel, numRecord, i, j, flag;
   double rV;

   // "command" used to store HTTP commands
   command = malloc(MAX_MSG_LEN);
   strcpy(command, "GET http://149.171.93.198/survey/8/ HTTP/1.0\n\n");

   // Initialise server address for UDP connection
   bzero(servAddr, sAddrLen);
   servAddr->sin_family = AF_INET;
   inet_pton(AF_INET, SERV_IP, &(servAddr->sin_addr.s_addr));
   servAddr->sin_port = htons(TCP_SERV_PORT);

   // TCP connection - survey questions
   sock = socket(AF_INET, SOCK_STREAM, 0);
   connect(sock, (struct sockaddr *)servAddr, sAddrLen);
   bytes = send(sock, command, strlen(command), 0);
   survey = malloc(SURVEY_SIZE);
   i = 0;
   while ((bytes = recv(sock, &survey[i], SURVEY_SIZE, 0)) > 0) {
      i = bytes; 
   }
   close(sock);

   // Count number of names
   s = survey;
   numStaff = 0;
   while ((s = strstr(s, "\"question\":")) != NULL) {
      s += strlen("\"question\":");
      numStaff++;
   }
   // Allocate array of pointers to index names and IDs
   surveyCont = malloc((numStaff + numStaff * 2) * sizeof(char *));
   s = (char *)surveyCont + numStaff * sizeof(char *);
   for (i = 0; i < numStaff; s += 2 * sizeof(char *), i++) {
      surveyCont[i] = (char **)s;
   }
   parseSurvey(survey, surveyCont);

   printf("\nPlease enter your privacy level: ");
   input = malloc(MAX_MSG_LEN);
   scanf("%s", input);
   privacyLevel = atoi(input);
   strcpy(command, "GET http://149.171.93.198/survey/8/");
   i = strlen(command);
   strcpy(&command[i], regMsg->userid);
   i = strlen(command);
   command[i++] = '/';
   strcpy(&command[i], servMsg->passwd);
   i = strlen(command);
   command[i++] = '/';
   strcpy(&command[i], input);
   i = strlen(command);
   strcpy(&command[i], "/session/ HTTP/1.0\n\n");

   // TCP connection - session creation
   sock = socket(AF_INET, SOCK_STREAM, 0);
   connect(sock, (struct sockaddr *)servAddr, sAddrLen);
   send(sock, command, strlen(command), 0);
   statResp = malloc(500);
   i = 0;
   while ((bytes = recv(sock, &statResp[i], 500, 0)) > 0) {
      i = bytes; 
   }
   close(sock);

   // Show session status
   s = strstr(statResp, "}");
   *s = '\0';
   s = strstr(statResp, "\"status\":");
   s += strlen("\"status\":");
   while (*s == ' ') {
      s++;
   }
   printf("\n%s\n", s);

   // User interface to upload responses
   flag = 0;
   while (flag == 0) {
      printf("\n\"list\": Alphabetical list of names and their ID numbers\n");
      printf("\"check\": List of names rated and their rating\n");
      printf("\"exit\": End rating session\n");
      printf("Otherwise, enter ID of lecturer you would like to rate: ");
      scanf("%s", input);
      if (strcmp(input, "list") == 0) {
         for (i = 0; i < numStaff; i++) {
            printf("%s, ID = %s\n", surveyCont[i][0], surveyCont[i][1]);
         }
      } else if (strcmp(input, "check") == 0) {
         strcpy(command, "GET http://149.171.93.198/survey/8/");
         i = strlen(command);
         strcpy(&command[i], regMsg->userid);
         i = strlen(command);
         command[i++] = '/';
         strcpy(&command[i], servMsg->passwd);
         i = strlen(command);
         strcpy(&command[i], "/responselist/ HTTP/1.0\n\n");

         // TCP connection - response list
         sock = socket(AF_INET, SOCK_STREAM, 0);
         connect(sock, (struct sockaddr *)servAddr, sAddrLen);
         send(sock, command, strlen(command), 0);
         // Assume that each entry is on average MAX_MSG_LEN,
         // remembering that space must also be allowed for the header.
         record = malloc(numStaff * MAX_MSG_LEN);
         i = 0;
         while ((bytes = recv(sock, &record[i], numStaff * MAX_MSG_LEN, 0)) > 0) {
            i = bytes;
         }
         close(sock);

         // Count number of records
         r = record;
         numRecord = 0;
         while ((r = strstr(r, "[\"")) != NULL) {
            r += strlen("[\"");
            numRecord++;
         }
         recordCont = malloc(numRecord * sizeof(char *));
         parseRecord(record, recordCont);
         // Show responses
         for (i = 0; i < numRecord; i++) {
            printf("%s\n", recordCont[i]);
         }

         free(recordCont);
         free(record);
      } else if (strcmp(input, "exit") == 0) {
         flag = 1;
      } else {
         for (i = 0; i < numStaff; i++) {
            // Match input ID with list of survey questions
            if (strcmp(input, surveyCont[i][1]) == 0) {
               printf("What would you like to give %s?\n", surveyCont[i][0]);
               printf("Rating: ");
               scanf("%s", input);
               j = atoi(input);
               //printf("input=%d, privacyLevel=%d\n", j, privacyLevel);
               rV = gaussianrvGen(privacyLevel);
               //printf("input + rV =%.16lf\n", (double)j+rV);
               sprintf(input, "%.16lf", (double)j + rV);
               //printf("uploaded input = %s\n", input);

               strcpy(command, "GET http://149.171.93.198/survey/");
               j = strlen(command);
               strcpy(&command[j], surveyCont[i][1]);
               j = strlen(command);
               command[j++] = '/';
               strcpy(&command[j], input);
               j = strlen(command);
               command[j++] = '/';
               strcpy(&command[j], regMsg->userid);
               j = strlen(command);
               command[j++] = '/';
               strcpy(&command[j], servMsg->passwd);
               j = strlen(command);
               strcpy(&command[j], "/response/ HTTP/1.0\n\n");

               // TCP connection - survey response
               sock = socket(AF_INET, SOCK_STREAM, 0);
               connect(sock, (struct sockaddr *)servAddr, sAddrLen);
               send(sock, command, strlen(command), 0);
               i = 0;
               while ((bytes = recv(sock, &statResp[i], 500, 0)) > 0) {
                  i = bytes; 
               }
               close(sock);

               // Show server confirmation of response
               r = strstr(statResp, "}");
               *r = '\0';
               r = strstr(statResp, "\"question\":");
               r += strlen("\"question\":");
               while (*r == ' ') {
                  r++;
               }
               s = strstr(r, "\",");
               s++;
               *s++ = '\0';
               s = strstr(s, "\"response\":");
               s += strlen("\"response\":");
               while (*s == ' ') {
                  s++;
               }
               printf("\nYou have uploaded a rating of %s for:\n%s.\n", s, r);

               // End loop since name matched
               i = numStaff;
            }
         }
      }
   }

   free(statResp);
   free(input);
   free(surveyCont);
   free(survey);
   free(command);
   free(servMsg);
   free(recvAddr);
   free(servAddr);
   free(regMsg);
   return 0;
}


void parseSurvey(char *survey, char ***surveyCont) {
   char *s;
   int numStaff, i, flag;

   // Index names and IDs in "survey" with "surveyCont"
   s = survey;
   numStaff = 0;
   while ((s = strstr(s, "\"question\":")) != NULL) {
      s += strlen("\"question\":");
      while (*s == ' ') {
         s++;
      }
      surveyCont[numStaff][0] = s;
      s = strstr(s, "\",");
      s++;
      *s++ = '\0';
      s = strstr(s, "\"id\":");
      s += strlen("\"id\":");
      while (*s == ' ') {
         s++;
      }
      surveyCont[numStaff][1] = s;
      s = strstr(s, "}");
      *s++ = '\0';
      numStaff++;
   }
   // Sort pointers by alphabetical order of names indexed
   flag = 1;
   while (flag == 1) {
      flag = 0;
      for (i = 0; i < numStaff - 1; i++) {
         if (strcmp(surveyCont[i][0], surveyCont[i + 1][0]) > 0) {
            s = surveyCont[i][0];
            surveyCont[i][0] = surveyCont[i + 1][0];
            surveyCont[i + 1][0] = s;
            s = surveyCont[i][1];
            surveyCont[i][1] = surveyCont[i + 1][1];
            surveyCont[i + 1][1] = s;
            flag = 1;
         }
      }
   }
}


void parseRecord(char *record, char **recordCont) {
   char *r;
   int numRecord;

   // Index responses
   r = strstr(record, "\"responses\":");
   numRecord = 0;
   while ((r = strstr(r, "[\"")) != NULL) {
      recordCont[numRecord] = ++r;
      r = strstr(r, "]");
      *r++ = '\0';
      numRecord++;
   }
}