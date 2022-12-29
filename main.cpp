#include <iostream>
//#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <queue>
#include <list>
#include <pthread.h>
#include <unistd.h>
#include <utility>

using namespace std;

int numOfCustomers = 0;
int completedOperations = 0;
ofstream outFile;

// 5 companies: 0:Kevin, 1:Bob, 2:Stuart, 3:Otto, 4:Dave
int get_company_id(const string& name){
    if(name == "Kevin")
        return 0;
    if(name == "Bob")
        return 1;
    if(name == "Stuart")
        return 2;
    if(name == "Otto")
        return 3;
    if(name == "Dave")
        return 4;
    return -1;
}
pthread_mutex_t company_mutexes[5];
int company_accounts[5];
map<int, string> companies;

struct Customer {       // Customer c(92, 1, "Kevin", 300);
    int customer_id;
    int sleep_time;
    int vending_machine_id;
    int company_id;
    int amount_paid;
    Customer(int id, int sleep, int vm_id, int company, int amount) :
            customer_id(id),
            sleep_time(sleep),
            vending_machine_id(vm_id),
            company_id(company),
            amount_paid(amount) {}
    Customer() : customer_id(-1) {}
};
Customer customers[301];
pthread_mutex_t customers_mutex;

// 10 different ticket vending machines, each have a list (queue) for Customers:
list<int> machines[10];
int machine_ids[10];    // each vending machine has an id
pthread_mutex_t machine_mutexes[10];

void *executeCustomer(void *param){
    Customer customer = *((Customer *)param);
    int customer_id = customer.customer_id;
    sleep(((double)customer.sleep_time) / 1000);
    int vm_id = customer.vending_machine_id;

    // Customer sends itself to the corresponding vending_machine's queue - if it has the right to access the queue:
    pthread_mutex_lock(&machine_mutexes[vm_id]);  // Lock the vending_machine mutex
    list<int> queue = machines[vm_id];
    queue.push_back(customer_id-1);
    machines[vm_id] = queue;
    pthread_mutex_unlock(&machine_mutexes[vm_id]);  // Unlock the vending_machine mutex
    pthread_exit(nullptr);
}

void *executeMachine(void *param){
    int vm_id = *(int*)param;
    while (completedOperations < numOfCustomers) {
        list<int> customer_queue = machines[vm_id];
        while (customer_queue.empty()) {
            customer_queue = machines[vm_id];
            if(completedOperations == numOfCustomers) {
                pthread_exit(nullptr);
            }
        }
        pthread_mutex_lock(&machine_mutexes[vm_id]);  // Lock th vending_machine mutex
        int customer_id = customer_queue.front();
        Customer customer = customers[customer_id];
        customer_queue.pop_front();
            int company_id = customer.company_id;
            pthread_mutex_lock(&company_mutexes[company_id]);  // Lock the company mutex
            company_accounts[company_id] += customer.amount_paid;
            // [vtm<vm_id>]: Customer<id>,<paid_amount>TL,<company_name>
            outFile << "[VTM" << vm_id+1 <<  "]: Customer" << customer_id+1 << "," << customer.amount_paid << "TL," << companies[company_id] << "\n";
            completedOperations++;
            pthread_mutex_unlock(&company_mutexes[company_id]);  // Unlock the company mutex
        machines[vm_id] = customer_queue;
        pthread_mutex_unlock(&machine_mutexes[vm_id]);  // Unlock the vending_machine mutex
    }
    pthread_exit(nullptr);
}

int main(int argc, char *argv[]) {

    // 5 companies: 0:Kevin, 1:Bob, 2:Stuart, 3:Otto, 4:Dave
    companies[0] = "Kevin";
    companies[1] = "Bob";
    companies[2] = "Stuart";
    companies[3] = "Otto";
    companies[4] = "Dave";

    // Read from the input file:
    string inputFile = argv[1];
    string inputLines;
    fstream ReadFile(inputFile);
    getline(ReadFile, inputLines);
    numOfCustomers = stoi(inputLines);  // will not exceed 300

    // Open the output file:
    string outputFile = inputFile.substr(0, inputFile.length() - 4) + "_log.txt";
    outFile.open(outputFile);

    // Create the Customer instances:
    for(int i = 0; i < numOfCustomers; i++) {
        // <sleep time>,<ticket vending machine instance>,<prepayment company name>,<amount>
        getline(ReadFile, inputLines);
        size_t pos;
        vector<string> split_customer;
        string token;
        while ((pos = inputLines.find(',')) != string::npos) {
            token = inputLines.substr(0, pos);
            split_customer.push_back(token);
            inputLines.erase(0, pos + 1);
        }
        split_customer.push_back(inputLines);
        Customer c(i+1, stoi(split_customer[0]),
                   stoi(split_customer[1]) - 1,
                   get_company_id(split_customer[2]),
                   stoi(split_customer[3]));
        pthread_mutex_lock(&customers_mutex);
        customers[i] = c;
        pthread_mutex_unlock(&customers_mutex);
    }
    ReadFile.close();

    // Create the Vending_Machine threads:
    pthread_t machineIdentifier[10];
    for(int i = 0; i < 10; i ++){
        machines[i] = list<int>();
        machine_ids[i] = i;
        pthread_mutex_init(&machine_mutexes[i], nullptr);
        pthread_create(&machineIdentifier[i], nullptr, executeMachine, &(machine_ids[i]));
    }

    // Create the Customer threads:
    pthread_t customerIdentifier[numOfCustomers];
    for(int i = 0; i < numOfCustomers; i++) {
        pthread_create(&customerIdentifier[i], nullptr, executeCustomer, &(customers[i]));
    }
    for (int i = 0; i < numOfCustomers; i++) {
        pthread_join(customerIdentifier[i], nullptr);
    }
    for (int i = 0; i < 10; i++) {
        pthread_join(machineIdentifier[i], nullptr);
        pthread_mutex_destroy(&machine_mutexes[i]);
    }

    if (outFile.is_open())
    {
        outFile << "[Main]: All prepayments are completed.\n";
        for(int i = 0; i < 5; i ++){
            outFile << "[Main]: " << companies[i] << ": " << company_accounts[i] << "TL\n";
        }
        outFile.close();
    }
    else
    {
        cerr << "Error opening file " << outputFile << endl;
    }
    return 0;
}
