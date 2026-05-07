#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

struct clientData {
    unsigned int acctNum;
    char lastName[15];
    char firstName[10];
    double balance;
};

struct transactionLog {
    unsigned int acctNum;
    char type[20];
    double amount;
    double balanceAfter;
    char timestamp[30];
};

unsigned int enterChoice(void);
void textFile(FILE *readPtr);
void updateRecord(FILE *fPtr);
void newRecord(FILE *fPtr);
void deleteRecord(FILE *fPtr);
void searchRecord(FILE *fPtr);
void showStatistics(FILE *fPtr);
void viewTransactionHistory(void);
void logTransaction(unsigned int acctNum, const char *type, double amount, double balanceAfter);
int  getValidAccountNumber(const char *prompt);
double getValidDouble(const char *prompt);
int  getValidString(const char *prompt, char *buf, int maxLen);
void clearInputBuffer(void);

int main(int argc, char *argv[])
{
    (void)argc;
    FILE *cfPtr;
    unsigned int choice;

    if ((cfPtr = fopen("credit.dat", "rb+")) == NULL) {
        printf("%s: File could not be opened.\n", argv[0]);
        exit(-1);
    }

    while ((choice = enterChoice()) != 7) {
        switch (choice) {
            case 1: textFile(cfPtr);        break;
            case 2: updateRecord(cfPtr);    break;
            case 3: newRecord(cfPtr);       break;
            case 4: deleteRecord(cfPtr);    break;
            case 5: searchRecord(cfPtr);    break;
            case 6: showStatistics(cfPtr);  break;
            default:
                puts("Incorrect choice. Please enter 1-7.");
                break;
        }
    }

    viewTransactionHistory();
    fclose(cfPtr);
    puts("\nProgram ended. Goodbye!\n");
    return 0;
}

void textFile(FILE *readPtr)
{
    FILE *writePtr;
    struct clientData client = {0, "", "", 0.0};

    if ((writePtr = fopen("accounts.txt", "w")) == NULL) {
        puts("File could not be opened.");
        return;
    }

    rewind(readPtr);
    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "----", "---------", "----------", "-------");

    while (fread(&client, sizeof(struct clientData), 1, readPtr) == 1) {
        if (client.acctNum != 0)
            fprintf(writePtr, "%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
    }

    fclose(writePtr);
    puts("accounts.txt created successfully.");
}

void updateRecord(FILE *fPtr)
{
    double transaction;
    struct clientData client = {0, "", "", 0.0};

    int account = getValidAccountNumber("Enter account to update ( 1 - 100 ): ");
    if (account == -1) return;

    fseek(fPtr, (account - 1) * sizeof(struct clientData), SEEK_SET);
    fread(&client, sizeof(struct clientData), 1, fPtr);

    if (client.acctNum == 0) {
        printf("Account #%d has no information.\n", account);
        return;
    }

    printf("\n%-6u%-16s%-11s%10.2f\n\n", client.acctNum, client.lastName, client.firstName, client.balance);

    transaction = getValidDouble("Enter charge ( + ) or payment ( - ): ");

    if (client.balance + transaction < 0) {
        printf("Transaction denied! Insufficient balance (%.2f). Cannot go below 0.\n", client.balance);
        return;
    }

    client.balance += transaction;
    printf("Updated: %-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);

    fseek(fPtr, -(long) sizeof(struct clientData), SEEK_CUR);
    fwrite(&client, sizeof(struct clientData), 1, fPtr);

    logTransaction(client.acctNum, transaction >= 0 ? "CREDIT" : "DEBIT", transaction, client.balance);
}

void newRecord(FILE *fPtr)
{
    struct clientData client = {0, "", "", 0.0};

    int accountNum = getValidAccountNumber("Enter new account number ( 1 - 100 ): ");
    if (accountNum == -1) return;

    fseek(fPtr, (accountNum - 1) * sizeof(struct clientData), SEEK_SET);
    fread(&client, sizeof(struct clientData), 1, fPtr);

    if (client.acctNum != 0) {
        printf("Account #%u already contains information.\n", client.acctNum);
        return;
    }

    if (!getValidString("Enter last name: ", client.lastName, 14))  return;
    if (!getValidString("Enter first name: ", client.firstName, 9)) return;

    client.balance = getValidDouble("Enter opening balance: ");
    if (client.balance < 0) {
        puts("Opening balance cannot be negative.");
        return;
    }

    client.acctNum = accountNum;
    fseek(fPtr, (client.acctNum - 1) * sizeof(struct clientData), SEEK_SET);
    fwrite(&client, sizeof(struct clientData), 1, fPtr);

    printf("Account #%u created for %s %s with balance %.2f\n", client.acctNum, client.firstName, client.lastName, client.balance);

    logTransaction(client.acctNum, "NEW_ACCOUNT", client.balance, client.balance);
}

void deleteRecord(FILE *fPtr)
{
    struct clientData client;
    struct clientData blankClient = {0, "", "", 0};

    int accountNum = getValidAccountNumber("Enter account number to delete ( 1 - 100 ): ");
    if (accountNum == -1) return;

    fseek(fPtr, (accountNum - 1) * sizeof(struct clientData), SEEK_SET);
    fread(&client, sizeof(struct clientData), 1, fPtr);

    if (client.acctNum == 0) {
        printf("Account %d does not exist.\n", accountNum);
        return;
    }

    char confirm;
    printf("Are you sure you want to delete account #%d (%s %s)? (y/n): ", accountNum, client.firstName, client.lastName);
    clearInputBuffer();
    confirm = getchar();

    if (tolower(confirm) != 'y') {
        puts("Deletion cancelled.");
        return;
    }

    logTransaction(client.acctNum, "DELETED", client.balance, 0.0);

    fseek(fPtr, (accountNum - 1) * sizeof(struct clientData), SEEK_SET);
    fwrite(&blankClient, sizeof(struct clientData), 1, fPtr);
    printf("Account #%d deleted successfully.\n", accountNum);
}

void searchRecord(FILE *fPtr)
{
    struct clientData client;
    int found = 0;
    char searchTerm[20];
    int searchChoice;

    printf("\nSearch by:\n");
    printf("  1 - Account Number\n");
    printf("  2 - Last Name\n");
    printf("  3 - First Name\n? ");

    if (scanf("%d", &searchChoice) != 1 || searchChoice < 1 || searchChoice > 3) {
        puts("Invalid search option.");
        clearInputBuffer();
        return;
    }

    if (searchChoice == 1) {
        int account = getValidAccountNumber("Enter account number to search ( 1 - 100 ): ");
        if (account == -1) return;

        fseek(fPtr, (account - 1) * sizeof(struct clientData), SEEK_SET);
        fread(&client, sizeof(struct clientData), 1, fPtr);

        if (client.acctNum != 0) {
            printf("\n%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
            printf("%-6s%-16s%-11s%10s\n",   "----", "---------", "----------", "-------");
            printf("%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
            found = 1;
        }
    } else {
        getValidString(searchChoice == 2 ? "Enter last name to search: " : "Enter first name to search: ", searchTerm, 19);

        for (int i = 0; searchTerm[i]; i++)
            searchTerm[i] = tolower(searchTerm[i]);

        rewind(fPtr);
        printf("\n%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
        printf("%-6s%-16s%-11s%10s\n",   "----", "---------", "----------", "-------");

        while (fread(&client, sizeof(struct clientData), 1, fPtr) == 1) {
            if (client.acctNum == 0) continue;

            char nameCopy[20];
            strncpy(nameCopy, searchChoice == 2 ? client.lastName : client.firstName, 19);
            nameCopy[19] = '\0';
            for (int i = 0; nameCopy[i]; i++)
                nameCopy[i] = tolower(nameCopy[i]);

            if (strstr(nameCopy, searchTerm) != NULL) {
                printf("%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
                found = 1;
            }
        }
    }

    if (!found)
        puts("No matching records found.");
    else
        puts("Search complete.");
}

void showStatistics(FILE *fPtr)
{
    struct clientData client;
    int totalAccounts  = 0;
    double totalBalance = 0.0;
    double maxBalance   = -1e18;
    double minBalance   =  1e18;
    struct clientData richest  = {0, "", "", 0.0};
    struct clientData poorest  = {0, "", "", 0.0};
    int negativeCount  = 0;

    rewind(fPtr);

    while (fread(&client, sizeof(struct clientData), 1, fPtr) == 1) {
        if (client.acctNum == 0) continue;
        totalAccounts++;
        totalBalance += client.balance;
        if (client.balance > maxBalance) { maxBalance = client.balance; richest = client; }
        if (client.balance < minBalance) { minBalance = client.balance; poorest = client; }
        if (client.balance < 0) negativeCount++;
    }

    if (totalAccounts == 0) { puts("No accounts found."); return; }

    printf("\n+------------------------------------------+\n");
    printf("|         BANK STATISTICS REPORT           |\n");
    printf("+------------------------------------------+\n");
    printf("|  Total Accounts   : %-21d|\n", totalAccounts);
    printf("|  Total Balance    : %-21.2f|\n", totalBalance);
    printf("|  Average Balance  : %-21.2f|\n", totalBalance / totalAccounts);
    printf("|  Highest Balance  : %-21.2f|\n", maxBalance);
    printf("|  Lowest Balance   : %-21.2f|\n", minBalance);
    printf("|  Negative Balances: %-21d|\n", negativeCount);
    printf("+------------------------------------------+\n");
    printf("|  Richest: #%-3u %-12s %-10s  |\n", richest.acctNum, richest.lastName, richest.firstName);
    printf("|  Poorest: #%-3u %-12s %-10s  |\n", poorest.acctNum, poorest.lastName, poorest.firstName);
    printf("+------------------------------------------+\n");
}

void logTransaction(unsigned int acctNum, const char *type, double amount, double balanceAfter)
{
    FILE *logPtr = fopen("transactions.log", "a");
    if (!logPtr) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(logPtr, "[%s] Acct#%-4d | %-14s | Amount: %+10.2f | Balance: %10.2f\n",
            timestamp, acctNum, type, amount, balanceAfter);

    fclose(logPtr);
}

void viewTransactionHistory(void)
{
    FILE *logPtr = fopen("transactions.log", "r");
    if (!logPtr) { puts("\nNo transaction history found."); return; }

    puts("\n+------------------------------------------------------------------+");
    puts("|                   TRANSACTION HISTORY                           |");
    puts("+------------------------------------------------------------------+");

    char line[200];
    int count = 0;
    while (fgets(line, sizeof(line), logPtr)) { printf("  %s", line); count++; }

    if (count == 0) puts("  (No transactions recorded this session)");

    fclose(logPtr);
}

void clearInputBuffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int getValidAccountNumber(const char *prompt)
{
    int account, attempts = 0;
    while (attempts < 3) {
        printf("%s", prompt);
        if (scanf("%d", &account) != 1) { puts("Invalid input. Please enter a number."); clearInputBuffer(); }
        else if (account < 1 || account > 100) { puts("Account number must be between 1 and 100."); }
        else return account;
        attempts++;
    }
    puts("Too many invalid attempts. Returning to menu.");
    return -1;
}

double getValidDouble(const char *prompt)
{
    double value;
    int attempts = 0;
    while (attempts < 3) {
        printf("%s", prompt);
        if (scanf("%lf", &value) == 1) return value;
        puts("Invalid input. Please enter a valid number.");
        clearInputBuffer();
        attempts++;
    }
    puts("Too many invalid attempts. Using 0.0.");
    return 0.0;
}

int getValidString(const char *prompt, char *buf, int maxLen)
{
    int attempts = 0;
    clearInputBuffer();
    while (attempts < 3) {
        printf("%s", prompt);
        if (fgets(buf, maxLen + 1, stdin)) {
            buf[strcspn(buf, "\n")] = '\0';
            if (strlen(buf) == 0) { puts("Input cannot be empty."); }
            else {
                int valid = 1;
                for (int i = 0; buf[i]; i++)
                    if (!isalpha(buf[i]) && buf[i] != '-' && buf[i] != '\'') { valid = 0; break; }
                if (valid) return 1;
                puts("Name must contain only letters.");
            }
        }
        attempts++;
    }
    puts("Too many invalid attempts.");
    return 0;
}

unsigned int enterChoice(void)
{
    unsigned int menuChoice;
    printf("\n+---------------------------------------+\n");
    printf("|          BANK ACCOUNT SYSTEM          |\n");
    printf("+---------------------------------------+\n");
    printf("|  1 - Print accounts to accounts.txt  |\n");
    printf("|  2 - Update an account                |\n");
    printf("|  3 - Add a new account                |\n");
    printf("|  4 - Delete an account                |\n");
    printf("|  5 - Search accounts                  |\n");
    printf("|  6 - View statistics                  |\n");
    printf("|  7 - End program                      |\n");
    printf("+---------------------------------------+\n? ");
    if (scanf("%u", &menuChoice) != 1) { clearInputBuffer(); return 0; }
    return menuChoice;
}
