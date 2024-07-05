#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_NAME_LENGTH 50
#define MAX_EVENTS 10
#define MAX_RESERVATIONS 100
#define MAX_CHAR 500

char buffer[BUFFER_SIZE];

typedef struct
{
    int id;
    char name[MAX_NAME_LENGTH];
    int tickets_available;
} Event;

typedef struct
{
    int event_id;
    int num_tickets;
    char customer_name[MAX_NAME_LENGTH];
    char phone_number[15];
    char password[20];
    char email[50];
} Reservation;

Event events[MAX_EVENTS] = {
    {1, "Cinéma", 20},
    {2, "Plage", 30},
    {3, "Concert", 10},
    {4, "Party", 15},
    {5, "Voyage", 50}};

int num_events = 5;
int num_reservations = 0;
Reservation reservations[MAX_RESERVATIONS];

int checkAvailability(int event_id, int num_tickets)
{
    for (int i = 0; i < num_events; i++)
    {
        if (events[i].id == event_id)
        {
            if (events[i].tickets_available >= num_tickets)
            {
                return 1; // Billets disponibles
            }
            else
            {
                return 0; // Billets épuisés
            }
        }
    }
    return -1; // Événement non trouvé
}

void makeReservation(const char *data, int client_fd)
{
    int event_id, num_tickets;
    char customer_name[MAX_NAME_LENGTH];
    char phone_number[15];
    char password[20];
    char email[50];
    char response[MAX_CHAR];

    if (sscanf(data, "%d,%d,%49[^,],%14[^,],%19[^,],%49s",
               &event_id, &num_tickets, customer_name, phone_number, password, email) == 6)
    {
        if (checkAvailability(event_id, num_tickets))
        {
            reservations[num_reservations].event_id = event_id;
            reservations[num_reservations].num_tickets = num_tickets;
            strcpy(reservations[num_reservations].customer_name, customer_name);
            strcpy(reservations[num_reservations].phone_number, phone_number);
            strcpy(reservations[num_reservations].password, password);
            strcpy(reservations[num_reservations].email, email);
            num_reservations++;
            for (int i = 0; i < num_events; i++)
            {
                if (events[i].id == event_id)
                {
                    events[i].tickets_available -= num_tickets;
                    break;
                }
            }
            snprintf(response, sizeof(response), "Réservation effectuée avec succès pour %s.\n", customer_name);
            send(client_fd, response, strlen(response), 0);
        }
        else
        {
            snprintf(response, sizeof(response), "Désolé, les billets ne sont pas disponibles pour cet événement ou la quantité demandée est trop élevée.\n");
            send(client_fd, response, strlen(response), 0);
        }
    }
    else
    {
        snprintf(response, sizeof(response), "Erreur lors de l'extraction des valeurs de la demande de réservation.\n");
        send(client_fd, response, strlen(response), 0);
    }
}

void cancelReservation(const char *data, int client_fd)
{
    char customer_name[MAX_NAME_LENGTH];
    sscanf(data, "%49s", customer_name);

    char response[BUFFER_SIZE];
    int count = 0;

    // Envoyer la liste des réservations au client
    for (int i = 0; i < num_reservations; i++)
    {
        if (strcmp(reservations[i].customer_name, customer_name) == 0)
        {
            count++;
            snprintf(buffer, BUFFER_SIZE, "%d. Événement : %s, Nombre de billets : %d\n",
                     count, events[reservations[i].event_id - 1].name, reservations[i].num_tickets);
            send(client_fd, buffer, strlen(buffer), 0);
        }
    }

    if (count == 0)
    {
        snprintf(response, BUFFER_SIZE, "Aucune réservation trouvée pour ce client.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }

    // Attendre la sélection du client
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0)
    {
        perror("recv");
        snprintf(response, BUFFER_SIZE, "Erreur lors de la réception de la sélection.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    buffer[bytes_received] = '\0';

    int choice;
    if (sscanf(buffer, "%d", &choice) != 1 || choice < 1 || choice > count)
    {
        snprintf(response, BUFFER_SIZE, "Choix invalide.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }

    // Annuler la réservation sélectionnée
    count = 0;
    for (int i = 0; i < num_reservations; i++)
    {
        if (strcmp(reservations[i].customer_name, customer_name) == 0)
        {
            count++;
            if (count == choice)
            {
                events[reservations[i].event_id - 1].tickets_available += reservations[i].num_tickets;
                for (int j = i; j < num_reservations - 1; j++)
                {
                    reservations[j] = reservations[j + 1];
                }
                num_reservations--;
                snprintf(response, BUFFER_SIZE, "Réservation annulée avec succès.\n");
                send(client_fd, response, strlen(response), 0);
                return;
            }
        }
    }
    snprintf(response, BUFFER_SIZE, "Erreur lors de l'annulation de la réservation.\n");
    send(client_fd, response, strlen(response), 0);
}

void displayCustomerReservations(const char *data, int client_fd)
{
    char customer_name[MAX_NAME_LENGTH];
    char password[20];
    sscanf(data, "%49[^,],%19s", customer_name, password);

    char response[BUFFER_SIZE];
    int found = 0;
    int valid_password = 0;

    for (int i = 0; i < num_reservations; i++)
    {
        if (strcmp(reservations[i].customer_name, customer_name) == 0)
        {
            found = 1;
            if (strcmp(reservations[i].password, password) == 0)
            {
                valid_password = 1;
                snprintf(response, BUFFER_SIZE, "- Événement : %s, Nombre de billets : %d\n",
                         events[reservations[i].event_id - 1].name, reservations[i].num_tickets);
                send(client_fd, response, strlen(response), 0);
            }
        }
    }

    if (!found)
    {
        snprintf(response, BUFFER_SIZE, "Client non trouvé.\n");
    }
    else if (!valid_password)
    {
        snprintf(response, BUFFER_SIZE, "Mot de passe incorrect.\n");
    }

    send(client_fd, response, strlen(response), 0);
}

void displayEvents(int client_fd)
{
    char response[BUFFER_SIZE];
    int offset = 0;

    for (int i = 0; i < num_events; i++)
    {
        Event *event = &events[i];
        offset += snprintf(response + offset, BUFFER_SIZE - offset, "Event ID: %d, Name: %s, Tickets: %d\n", event->id, event->name, event->tickets_available);
    }

    send(client_fd, response, strlen(response), 0);
}

void deleteEvent(const char *data, int client_fd)
{
    int event_id;
    char response[BUFFER_SIZE];

    if (sscanf(data, "%d", &event_id) == 1)
    {
        int found = 0;

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].id == event_id)
            {
                found = 1;

                for (int j = i; j < num_events - 1; j++)
                {
                    events[j] = events[j + 1];
                }
                num_events--;
                snprintf(response, sizeof(response), "Événement supprimé avec succès.\n");
                send(client_fd, response, strlen(response), 0);
                return;
            }
        }

        if (!found)
        {
            snprintf(response, sizeof(response), "Événement non trouvé.\n");
            send(client_fd, response, strlen(response), 0);
        }
    }
    else
    {
        snprintf(response, sizeof(response), "Erreur lors de l'extraction des valeurs de la demande de suppression.\n");
        send(client_fd, response, strlen(response), 0);
    }
}

void *client_handler(void *arg)
{
    int client_fd = *((int *)arg);
    free(arg);

    while (1)
    {
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "LIST_EVENTS", 11) == 0)
        {
            displayEvents(client_fd);
        }
        else if (strncmp(buffer, "RESERVE", 7) == 0)
        {
            makeReservation(buffer + 8, client_fd);
        }
        else if (strncmp(buffer, "CANCEL", 6) == 0)
        {
            cancelReservation(buffer + 7, client_fd);
        }
        else if (strncmp(buffer, "SHOW_RESERVATIONS", 17) == 0)
        {
            displayCustomerReservations(buffer + 18, client_fd);
        }
        else if (strncmp(buffer, "DELETE_EVENT", 12) == 0)
        {
            deleteEvent(buffer + 13, client_fd);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Commande inconnue.\n");
            send(client_fd, buffer, strlen(buffer), 0);
        }
    }

    close(client_fd);
    return NULL;
}

int main()
{
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Serveur démarré sur le port %d\n", SERVER_PORT);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*client_fd == -1)
        {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, client_fd) != 0)
        {
            perror("pthread_create");
            free(client_fd);
        }

        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
