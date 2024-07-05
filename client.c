#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_NAME_LENGTH 50
#define MAX_PHONE_LENGTH 15
#define MAX_PASSWORD_LENGTH 20
#define MAX_EMAIL_LENGTH 50

void display_events(const char *data)
{
    printf("Événements disponibles :\n");
    const char *delimiter = "\n";
    char *line = strtok((char *)data, delimiter);

    while (line != NULL)
    {
        int id, tickets_available;
        char name[MAX_NAME_LENGTH];

        if (sscanf(line, "Event ID: %d, Name: %49[^,], Tickets: %d", &id, name, &tickets_available) == 3)
        {
            printf("- ID: %d, Nom: %s, Tickets disponibles: %d\n", id, name, tickets_available);
        }

        line = strtok(NULL, delimiter);
    }
}

void display_menu()
{
    printf("\nMenu :\n");
    printf("1. Afficher les événements disponibles\n");
    printf("2. Réserver des billets\n");
    printf("3. Annuler une réservation\n");
    printf("4. Afficher mes réservations\n");
    printf("5. Supprimer un événement\n");
    printf("6. Quitter\n");
    printf("Choix : ");
}

void send_cancel_request(int client_fd, const char *customer_name)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "CANCEL,%s", customer_name);
    send(client_fd, buffer, strlen(buffer), 0);

    // Recevoir la liste des réservations
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        printf("%s\n", buffer);

        // Demander à l'utilisateur de sélectionner une réservation à annuler
        printf("Sélectionnez le numéro de la réservation à annuler : ");
        int choice;
        scanf("%d", &choice);

        snprintf(buffer, BUFFER_SIZE, "%d", choice);
        send(client_fd, buffer, strlen(buffer), 0);

        // Recevoir la réponse du serveur après l'annulation
        bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';
            printf("%s\n", buffer);
        }
    }
    else
    {
        printf("Aucune réservation trouvée pour ce client.\n");
    }
}

void send_show_reservations_request(int client_fd, const char *customer_name, const char *password)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "SHOW_RESERVATIONS,%s,%s", customer_name, password);
    send(client_fd, buffer, strlen(buffer), 0);

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        printf("%s\n", buffer);
    }
}

void send_delete_event_request(int client_fd, int event_id)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "DELETE_EVENT,%d", event_id);
    send(client_fd, buffer, strlen(buffer), 0);

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        printf("%s\n", buffer);
    }
}

int main()
{
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int event_id, num_tickets;
    char customer_name[MAX_NAME_LENGTH];
    char phone_number[15];
    char password[20];
    char email[50];
    char choice;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connecté au serveur de réservation.\n");

    while (1)
    {
        display_menu();
        scanf(" %c", &choice);

        switch (choice)
        {
        case '1':
            if (send(client_fd, "LIST_EVENTS", strlen("LIST_EVENTS"), 0) == -1)
            {
                perror("send");
                break;
            }

            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                perror("recv");
                break;
            }

            buffer[bytes_received] = '\0';
            display_events(buffer);
            break;

        case '2':
            printf("Sélectionnez l'événement (ID) : ");
            scanf("%d", &event_id);

            printf("Nombre de billets à réserver : ");
            scanf("%d", &num_tickets);

            printf("Nom du client : ");
            scanf(" %[^\n]", customer_name);

            printf("Numéro de téléphone (+229xxxxxxxx) : ");
            scanf(" %s", phone_number);

            printf("Mot de passe : ");
            scanf(" %s", password);

            printf("Adresse e-mail : ");
            scanf(" %s", email);

            snprintf(buffer, BUFFER_SIZE, "RESERVE,%d,%d,%s,%s,%s,%s",
                     event_id, num_tickets, customer_name, phone_number, password, email);

            if (send(client_fd, buffer, strlen(buffer), 0) == -1)
            {
                perror("send");
                break;
            }

            bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                perror("recv");
                break;
            }

            buffer[bytes_received] = '\0';
            printf("%s\n", buffer);
            break;

        case '3':
            printf("Nom du client : ");
            scanf(" %[^\n]", customer_name);
            send_cancel_request(client_fd, customer_name);
            break;

        case '4':
            printf("Nom du client : ");
            scanf(" %[^\n]", customer_name);
            printf("Mot de passe : ");
            scanf(" %s", password);
            send_show_reservations_request(client_fd, customer_name, password);
            break;

        case '5':
            printf("ID de l'événement à supprimer : ");
            scanf("%d", &event_id);
            send_delete_event_request(client_fd, event_id);
            break;

        case '6':
            close(client_fd);
            printf("Déconnexion...\n");
            exit(0);

        default:
            printf("Choix invalide. Veuillez réessayer.\n");
            break;
        }
    }

    close(client_fd);
    return 0;
}
