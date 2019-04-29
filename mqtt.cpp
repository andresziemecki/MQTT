#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include "queue.h"
#include <list>

using namespace std;

using TopicName = string;
using TopicValue = string;

mutex mutex_cout;

/* **************************** MESSAGE SECTION ******************************* 
*******************************************************************************/

class Message {
    public:
        enum class Type { CONNECT, CONNACK, PUBLISH, SUBSCRIBE, UNSUBSCRIBE, DISCONNECT };
        Type getType() const{
            return type;
        }
        virtual Message *clone() const = 0;
        virtual ~Message(){}
    protected:
        Type type;
};

class ConnectMsg : public Message { 
    private:
    string username;
    string password;
    public:
        ConnectMsg(string user, string pass):username(user), password(pass){
            Message::type = Message::Type::CONNECT;
        }
        ~ConnectMsg(){}
        Message *clone() const{
            ConnectMsg *M = new ConnectMsg(username, password); //SI PONGO ESTO ME DEJA DE ANDAR
            return dynamic_cast<Message *>(M);
        }
        string getUsername() const{
            return username;
        }
        string getPassword() const{
            return password;
        }
};

class ConnAckMsg : public Message {
    public:
        enum class Status { CONNECTION_OK, LOGIN_ERROR};
        ConnAckMsg(Status b){
            Message::type = Message::Type::CONNACK;
            if(b==Status::CONNECTION_OK)
                status = Status::CONNECTION_OK;
            else
                status = Status::LOGIN_ERROR;
        }
        ~ConnAckMsg(){}
        Message *clone() const {
            ConnAckMsg *M = new ConnAckMsg(status);
            return (Message *)M;
        }
        
        Status getStatusType() const {
            return status;
        }
    private:
        Status status;
};

class PublishMsg : public Message {
    private:
        TopicName topic;
        TopicValue value;
        bool retain;
    public:
        PublishMsg(TopicName n, TopicValue v, bool b=false):topic(n), value(v), retain(b){
            Message::type = Message::Type::PUBLISH;
        }
        ~PublishMsg(){}
        Message *clone() const{
            PublishMsg *M = new PublishMsg(topic, value, retain);
            return (Message *)M; 
        }
        TopicName getTopic() const{return topic;}
        TopicValue getValue() const {return value;}
        bool getRetain() const {return retain;}
};

class SubscribeMsg : public Message {
        TopicName topic;
    public:
        SubscribeMsg(TopicName n):topic(n){
            Message::type = Message::Type::SUBSCRIBE;
        }
        ~SubscribeMsg(){}
        Message *clone() const{
            SubscribeMsg *M = new SubscribeMsg(topic);
            return (Message *)M;
        }
        TopicName getTopic (void) const{return topic;} 
};

class UnsubscribeMsg : public Message {
    private:
        TopicName topic;
    public:
        UnsubscribeMsg(TopicName t):topic(t){
            Message::type = Message::Type::UNSUBSCRIBE;
        }
        ~UnsubscribeMsg(){}
        Message *clone() const{
        UnsubscribeMsg *M = new UnsubscribeMsg(topic);
            return (Message *)M;
        }
        TopicName getTopic() const{return topic;}
};
 
class DisconnectMsg : public Message {
    public:
    DisconnectMsg(){
        Message::type = Message::Type::DISCONNECT;
    }
    ~DisconnectMsg(){}
    Message *clone() const{
        DisconnectMsg *M = new DisconnectMsg();
        return (Message *)M; 
    }
};

/* **************************** CLIENT SECTION ******************************* 
******************************************************************************/

class ClientOpsIF {
    public:
        virtual void recvMsg(const Message &) = 0;
};

class BrokerOpsIF {
    public:
        BrokerOpsIF()=default;
        virtual ~BrokerOpsIF(){}
        virtual void sendMsg(const Message &) = 0; //SAQUE EL CONST
};

//  Declaraciones necesarias para la clase Client
class Broker;
struct usr_pass{
    string username;
    string password;
    usr_pass(){}
    usr_pass(string i, string j):username(i), password(j){}
    usr_pass(const usr_pass &B){
        username = B.username;
        password = B.password;
    }
};

class Client : BrokerOpsIF {
    protected:
        list<TopicName> Lista_Topics_subscriptos;
        Broker *B;
        thread th;
        ClientOpsIF *cif;
        cola<Message *, 100> recvQueue; // hasta 100 mensajes podra retener el client
    public:
        bool suscr;
        TopicName Client_TopicName;
        usr_pass u_p; // usuario y contraseña del SimClient registrado en este cliente
        Client(ClientOpsIF *i); // Ejecuta el thread del client con startClient
        ~Client(); // Espera a que el thread sea terminado antes de destruirse
        ClientOpsIF *getCif(){return cif;}
        void getInstanceOfBroker(void); // Obtiene la referencia del broker y la pone en B
        void sendMsg(const Message &m); // Todos los mensajes que le llegan al client lo pone en la cola
        void startClient(void); // Saca los mensajes de la cola y los va procesando
        void Register_User(ConnectMsg *M); //Registra el SimClient si no ha sido registrado
        bool check_username (ConnectMsg *M) const; // Si ya ha sido registrado Verifica que su usr y pass
        // Inserta un topico (retained) y su valor en el map(RetainedTopics) del broker y le avisa a todos los subscriptos a ese topic el valor
        void insert_retainedTopic (TopicName TN, TopicValue TV);
        void Assign_Client_TopicName(TopicName TN); // Asigna el topic name del retained topic para ayudar a destruir este objeto
        void insert_subscriber(TopicName T_N); // Agrega un subscriptor y el topico que se subscribio al map(subcritpso) del broker
        // Function que le manda publish a todos los simclient que estan registrados a ese topic
        void warn_everybody_thisTopic(TopicName Tn, TopicValue Tv) const; 
        void unsuscribe_topic(TopicName Tn); // Funcion que desuscribe de un topic a un SimClient
        void unsuscribe_all_topics(); // Funcion que unsuscribe a todos los topics cuando se desconecta el simclient
};


/* **************************** BROKER SECTION *******************************
*****************************************************************************/
class ParametrosDelSimClient;

class Broker {
    private:
        // Tengo un registro de las IF y sus clientes crrespondtes para no crear Clients de mas
        unordered_map<ClientOpsIF *, Client *> clients; 
        // Contenedor que tiene retiene los topic Name y sus respectivos valores de todos los Clientes
        unordered_map<TopicName , TopicValue> RetainedTopics; 
        // Contenedor que tiene todos los clientes subscriptos para un dado topic
        unordered_multimap<TopicName, Client *> subscriptos;
        static Broker* instance;
        Broker();
        ~Broker();
    public:
        // Cada uno de estos mutex sirve para manipular cada uno de los objetos
        mutex m_clients;
        mutex m_retainedTopics;
        mutex m_subscriptos;
        static Broker* getInstance();
        // Crea un Client para darselo al SimClient, si este ya existe le pasa el puntero
        BrokerOpsIF * registerClient(ClientOpsIF *c); 
        void ClientDestruction(Client *, TopicName); // funcion que eliminara a un cliente de los brokers containers
        // Las siguientes son funciones auxiliares que necesita Client para llevar a cabo el objetivo
        void Broker_insert_subscriber(TopicName, Client *);
        void Broker_Insert_Retained_Topic (TopicName, TopicValue);
        void Broker_warn_everybody_thisTopic (TopicName Tn, TopicValue Tv);
        void Broker_unsuscribe_topic(TopicName, Client *);
};


/* **************************** SIMCLIENT SECTION *******************************
**********************************************************************************/

// Parámetros que necesita un SimPublisher para ejecutar en el main si desea subscribirse o publicar
class ParametrosDelSimClient{
    public:
    TopicName topic_name;
    bool subscriptor;
    int topic_value; //topicvalue
    int error;
    bool retain; //retain
    unsigned int time_conection;
    ParametrosDelSimClient(){}
    ParametrosDelSimClient(TopicName,bool, int, int, bool, unsigned int);
    ParametrosDelSimClient(const ParametrosDelSimClient &A);
};

ParametrosDelSimClient::ParametrosDelSimClient(TopicName TN,bool s=false, int TV=0, int e=0, bool r=false, unsigned int t=10){
    topic_name=TN;
    subscriptor=s;
    topic_value=TV;
    error = e;
    retain = r;
    time_conection = t;
}

ParametrosDelSimClient::ParametrosDelSimClient(const ParametrosDelSimClient &A){
    topic_name = A.topic_name;
    subscriptor = A.subscriptor;
    topic_value = A.topic_value;
    error=A.error;
    retain=A.retain;
    time_conection=A.time_conection;
}

class SimClient : public ClientOpsIF {
    protected:
        Broker &broker;
        usr_pass usuario_pass;
        virtual void runSim()=0;
    public:
        thread simth; // TUVE QUE COLOCAR EL THREAD EN PUBLICO 
        ParametrosDelSimClient Par_del_client; // Si es publisher, suscriptor, etc
        SimClient(Broker &b, usr_pass &uP); // inicializarlo con broker y usario-contraseña
        ~SimClient(){}
        void start(ParametrosDelSimClient &); // Funcion que comienza la ejecucion(thread) del SimClient
        virtual void recvMsg(const Message &){}
};


class SimPublisher : public SimClient {
        void runSim(); // funcion que toma el thread en start para ejecutar
    public:
        bool connack_value; // Variable auxiliar para comunicar al thread si el connack fue correcto o erroneo
        mutex mmutex;
        condition_variable cond_var;
        bool ready; // variable auxiliar para salir del bucle while(wait for_...) del thread
        BrokerOpsIF *brops;

        SimPublisher(Broker &j, usr_pass &uP);
        ~SimPublisher();
        
        void Subscribirse(TopicName tn); // funcion que llama runsim si el SimClient es subscriptor
        void Publish (void); // funcion que llama runsim si es SimClient es publisher
        void SubscribeNewTopic(TopicName tn); // funcion que permite desde el main subsscribirse a mas de un topic
        void UnsuscribeTopic(TopicName tn); // Se desinscribe de un topico
        void Disconnect(void); // Se descnoecta
        void recvMsg(const Message &m); // Funcion que recive los mensajes del ClientOpsIF y los procesa
        void recvConnack(ConnAckMsg *M); // Funcion que llama recvMsg si es un connack
        void recvPublishMsg(PublishMsg *M) const; // Funcion que llama recvMsg si es un publish
};

// Inicializamos un singleton broker global
Broker* Broker::instance = 0;
Broker::Broker(){}; 

/* **************************** MAIN PROGRAM ******************************* 
****************************************************************************/

int main(void){
    
    Broker* broker = Broker::getInstance();

    //ParametrosDelSimClient {Parametro, Subscriptor?, Valor, error, Retener valor, tiempo de ejec}
    usr_pass a_z("\033[1;31mandres\033[0m", "ziemecki");
    ParametrosDelSimClient p1 = {"Presion", false, 1000, 100, true, 15};
    SimPublisher andy(*broker, a_z);
    andy.start(p1);

    this_thread::sleep_for (std::chrono::seconds(2));

    usr_pass h_f("\033[1;36mHoracio\033[0m", "Fontanini");
    ParametrosDelSimClient p3 = {"Temperatura", false, 30, 10, false, 10};
    SimPublisher fonta(*broker, h_f);
    fonta.start(p3);

    usr_pass e_t("\033[1;32mEduardo\033[0m", "Tapia");
    ParametrosDelSimClient p2 = {"Presion", true};
    SimPublisher edu(*broker, e_t);
    edu.start(p2);

    this_thread::sleep_for (std::chrono::seconds(3));

    usr_pass d_b("\033[1;33mDario\033[0m", "Balmaceda");
    ParametrosDelSimClient p4 = {"Temperatura", true};
    SimPublisher dari(*broker, d_b);
    dari.start(p4);

    edu.SubscribeNewTopic("Temperatura");

    this_thread::sleep_for (std::chrono::seconds(5));

    edu.UnsuscribeTopic("Presion");
    edu.UnsuscribeTopic("Presion");
    edu.UnsuscribeTopic("Temperatura");

    this_thread::sleep_for (std::chrono::seconds(2));

    // dari.UnsuscribeTopic("Temperatura");
    edu.Disconnect();
    dari.Disconnect();

    return 0;
}

/* **************************** FUNCTIONS OF CLIENT SECTION ******************************* 
*******************************************************************************************/


Client::Client(ClientOpsIF *i):cif(i){
    u_p = {"$", "$"}; // Cuando se crea el cliente se le asigna un usuario y contraseña arbitrario
    getInstanceOfBroker(); // Llama a esta funcion para asignar a *B el Broker existente
    this->th = thread(&Client::startClient, this); // Se inicia un thread apenas se construye
}

Client::~Client(){ 
    this->th.join(); // Espero a que finalice el thread en el constructor
}

void Client::sendMsg(const Message &m) {
    recvQueue.push(m.clone()); // Cuando llega un mensae lo pongo en la cola
}

void Client::startClient(void){
    Message *m;
    bool disconnect = false;
    do{ 
        recvQueue.pop(&m); 
        switch (m->getType()){
            case Message::Type::CONNECT: {
                ConnectMsg *M = (ConnectMsg *)m;
                if (u_p.username == "$" && u_p.password=="$"){
                    Register_User(M);
                    mutex_cout.lock();
                    cout << "From SimClient\tMessage: CONNECT\tfrom: "<< M->getUsername() << " Registered"<< endl;
                    mutex_cout.unlock();
                    cif->recvMsg(ConnAckMsg(ConnAckMsg::Status::CONNECTION_OK));
                    }
                else if (check_username(M)){
                    mutex_cout.lock();
                    cout << "From SimClient\tMessage: CONNECT\tfrom: "<< M->getUsername() << "Logined" << endl;
                    mutex_cout.unlock();
                    cif->recvMsg(ConnAckMsg(ConnAckMsg::Status::CONNECTION_OK));}
                else
                    cif->recvMsg(ConnAckMsg(ConnAckMsg::Status::LOGIN_ERROR));
                delete(M);
                break;}
            case Message::Type::CONNACK: 
                break; // No hay connack hacia el Client, por eso no hay nada
            case Message::Type::PUBLISH:{
                PublishMsg *M = (PublishMsg *)m;
                mutex_cout.lock();
                cout << "From SimClient\tMessage: PUBLISH\tfrom: "<< u_p.username<<"\t" << "Topic " << M->getTopic() << ": " << M->getValue() << endl;
                mutex_cout.unlock();
                if (M->getRetain())
                    insert_retainedTopic (M->getTopic(), M->getValue());
                else
                    warn_everybody_thisTopic(M->getTopic(), M->getValue());
                delete(M);
                break;} 
            case Message::Type::SUBSCRIBE: {
                SubscribeMsg *M = (SubscribeMsg *)m;
                mutex_cout.lock();
                cout << "From SimClient\tMessage: SUBSCRIBE\tfrom: "<< u_p.username<<"\t" << "Topic "<< M->getTopic()<< endl;
                mutex_cout.unlock();
                insert_subscriber(M->getTopic());
                delete(M);
                break; }
            case Message::Type::UNSUBSCRIBE:{
                    UnsubscribeMsg *M = (UnsubscribeMsg *)m;
                    mutex_cout.lock();
                    cout << "From SimClient\tMessage: UNSUCRIBE\tfrom: "<< u_p.username<<"\t" << "Topic "<< M->getTopic() << endl;
                    mutex_cout.unlock();
                    unsuscribe_topic(M->getTopic());
                    delete(M);
                break; }
            case Message::Type::DISCONNECT:{
                DisconnectMsg *M = (DisconnectMsg *)m;
                mutex_cout.lock();
                cout << "From SimClient\tMessage: DISCONNECT\tfrom: "<< u_p.username<<endl;
                mutex_cout.unlock();
                disconnect = true;
                delete(M);
                break;}
        }
    }while(!disconnect);
    unsuscribe_all_topics(); 
    std::thread thr (&Broker::ClientDestruction,B,this, Client_TopicName);
    thr.detach();
}

void Client::Register_User(ConnectMsg *M){
    u_p.username = M->getUsername();
    u_p.password = M->getPassword();
    return;
}

bool Client::check_username (ConnectMsg *M) const {
    return (u_p.username == M->getUsername() && u_p.password == M->getPassword());
}

void Client::insert_retainedTopic (TopicName TN, TopicValue TV){
    if(Client_TopicName != TN)
        Assign_Client_TopicName(TN);
    B->Broker_Insert_Retained_Topic(TN,TV);
    warn_everybody_thisTopic(TN, TV);
}

void Client::Assign_Client_TopicName(TopicName TN){
    Client_TopicName = TN;
    return; 
}

void Client::insert_subscriber(TopicName TN){
    list<TopicName>::iterator it=Lista_Topics_subscriptos.begin();
    for (; it!=Lista_Topics_subscriptos.end(); ++it){}
    if( it == Lista_Topics_subscriptos.end()){
            Lista_Topics_subscriptos.insert(Lista_Topics_subscriptos.begin(),TN);
            B->Broker_insert_subscriber(TN, this); // subscribir el Client en el map(subscriptos) del broker
        }
    return;
}

void Client::warn_everybody_thisTopic(TopicName Tn, TopicValue Tv) const{
    B->Broker_warn_everybody_thisTopic (Tn, Tv);
}

void Client::getInstanceOfBroker(void){
    B = Broker::getInstance();
}

void Client::unsuscribe_topic(TopicName TN){
    for (list<TopicName>::iterator it=Lista_Topics_subscriptos.begin(); it!=Lista_Topics_subscriptos.end(); ++it){
        if(*it == TN){
            it=Lista_Topics_subscriptos.erase(it);
            B->Broker_unsuscribe_topic(TN, this);
        }
    }
    
}
 
void Client::unsuscribe_all_topics(){
    while (!Lista_Topics_subscriptos.empty())
    {
        unsuscribe_topic(Lista_Topics_subscriptos.front());
        //Lista_Topics_subscriptos.pop_front();
    }
}
/* **************************** FUNCTIONS OF BROKER SECTION ******************************* 
******************************************************************************************/

Broker* Broker::getInstance(){
    if (instance == 0){
        instance = new Broker();
    }
    return instance;
}

Broker::~Broker(){

    unordered_map<ClientOpsIF *, Client *> ::iterator it = clients.begin();
    while(it != clients.end()){
	    delete(it->second);
	    it++;
    }
}

BrokerOpsIF * Broker::registerClient(ClientOpsIF *c){
    m_clients.lock();
    unordered_map<ClientOpsIF *, Client *>::const_iterator got = clients.find (c);
    // if Client is not register, register it in the map(clients) of broker and give it the pointer
    if ( got == clients.end() || got->second == nullptr){
        Client *Br = new Client(c);
        clients.insert ( {c,Br} );
        m_clients.unlock();
        return (BrokerOpsIF *) Br;
    } 
    else{ //if it is registered, and has a client, give it to him
        m_clients.unlock();
        return (BrokerOpsIF *)got->second;
    }
    
}

void Broker::ClientDestruction(Client * C, TopicName TN){
    this_thread::sleep_for (std::chrono::seconds(5));
    RetainedTopics.erase(TN);
    delete(C);
    return;
}

void Broker::Broker_Insert_Retained_Topic (TopicName TN, TopicValue TV){
    m_retainedTopics.lock();
    RetainedTopics[TN]=TV;
    m_retainedTopics.unlock();
    return;
}

void Broker::Broker_warn_everybody_thisTopic (TopicName Tn, TopicValue Tv){
    m_subscriptos.lock();
    auto its = subscriptos.equal_range(Tn); // encuentra todos los Client para un dado TopicName
    for (auto it = its.first; it != its.second; ++it) {
        // Le manda un publish a cada SimClient con el value del topic encontrado
        it->second->getCif()->recvMsg(PublishMsg(Tn, Tv)); // Si reemplazo cif por getCif() el proceso no devuelve todo el output
    }
    m_subscriptos.unlock();
}

void Broker::Broker_unsuscribe_topic(TopicName TN, Client *C){
    m_subscriptos.lock();
    // para un dado TopicName, encuentra todos los Clientes del map(subscriptores) del broker y los pone en its
    auto its = subscriptos.equal_range(TN);
    // Busco el client "This" entre los Client en "its" y lo remuevo del map
    auto it = its.first;
    for (; it != its.second && it->second != C; ++it){}
    if(it->second==C) //NOSE PORQUE ESTO NO ANDA SI NO ESTOY SUBSCRIPTO(SEG FAULT)
        subscriptos.erase(it);
    m_subscriptos.unlock();
    return;
}

void Broker::Broker_insert_subscriber(TopicName TN, Client *C){
    m_subscriptos.lock();
    subscriptos.insert({TN,C});
    m_subscriptos.unlock();
    m_retainedTopics.lock();
    auto it = RetainedTopics.find(TN);
    if ( !(it == RetainedTopics.end())) // si ya hay un topic de esto retenido, mandarlo al SimClient
        C->getCif()->recvMsg(PublishMsg(it->first, it->second));
    m_retainedTopics.unlock();
    return;
}


/* **************************** FUNCTIONS OF SIMCLIENT SECTION ******************************* 
*********************************************************************************************/

SimClient::SimClient(Broker &b, usr_pass &uP):broker(b){
    usuario_pass=uP;
}

SimPublisher::SimPublisher(Broker &j, usr_pass &uP):SimClient(j, uP){
    ready = false;
    usuario_pass=uP;
}
 
SimPublisher::~SimPublisher(){
    std::this_thread::sleep_for (std::chrono::seconds(2));
    simth.join();
}

void SimClient::start(ParametrosDelSimClient &PdC){
    Par_del_client = PdC;
    //simth = thread{&SimClient::runSim, this};
    simth = move(thread{&SimClient::runSim, this});
}

void SimPublisher::runSim(){
    // Registrar el cliente
    brops = broker.registerClient(this);
    // Esperar un rato a que empiecen las conecciones
    std::this_thread::sleep_for (std::chrono::seconds(rand() % 3));
    
    // Mandar el connect
    brops->sendMsg(ConnectMsg(usuario_pass.username, usuario_pass.password));
    // esperar por CONNACK sin errores
    std::unique_lock<std::mutex> lock(mmutex);
    while(!connack_value){ 
        if(cond_var.wait_for(lock, std::chrono::seconds(5), [&](){return ready;})){
            break;
        }
        else{
            cout << "Loggin Error, no response of the broker" << endl;
            break;
        }
    }

    if (connack_value)
        ;
    else{
        mutex_cout.lock();
        cout << "Connack not recive -> Error" << endl; // DISCONNECT
        mutex_cout.unlock();
        return;
    }
    // Si el cliente es subscriptor que se subscriba a algo, sino que publique
    if(!Par_del_client.subscriptor)
        Publish();
    else
        Subscribirse(Par_del_client.topic_name);
}

void SimPublisher::Subscribirse(TopicName tn){
    brops->sendMsg(SubscribeMsg(tn));
}

void SimPublisher::Publish (void){
        // Empezar a publicar valores
    for (unsigned int i=0; i<Par_del_client.time_conection; ++i){
        std::this_thread::sleep_for (std::chrono::seconds(1));
        brops->sendMsg(PublishMsg(Par_del_client.topic_name, to_string(rand()%Par_del_client.error - Par_del_client.error/2 + Par_del_client.topic_value), Par_del_client.retain));
    }
    // Disconnect luego del tiempo de vida
    brops->sendMsg(DisconnectMsg{});
}

void SimPublisher::SubscribeNewTopic(TopicName tn){
    if (Par_del_client.subscriptor)
        brops->sendMsg(SubscribeMsg(tn));
    else{
        mutex_cout.lock();
        cout << "Error, you're only a Publisher" << endl;
        mutex_cout.unlock();
    }
}

void SimPublisher::UnsuscribeTopic(TopicName tn){
    brops->sendMsg(UnsubscribeMsg(tn));
}

void SimPublisher::Disconnect(void){
    brops->sendMsg(DisconnectMsg());
}

void SimPublisher::recvMsg(const Message &m){
    switch (m.getType()){
        case Message::Type::CONNACK:
        {
            ConnAckMsg *M = (ConnAckMsg *)&m;
            recvConnack(M);
            break;
        }
        case Message::Type::PUBLISH:{
            PublishMsg *M = (PublishMsg *)&m;
            recvPublishMsg(M);
            break;
        }
        default:{ // Cualquier otro tipo de mensae recibido es indeseado
            mutex_cout.lock();
            cout << "Error al recibir Mensaje del Client: " << usuario_pass.username << endl;
            mutex_cout.unlock();
            return;}
    }
}

void SimPublisher::recvConnack(ConnAckMsg *M){
    
    switch ((int)(M->getStatusType())){
        case 0: // LOGIN_OK
        {
            mutex_cout.lock();
            cout << "From Client\tMessage: CONNACK\tto: "<< usuario_pass.username << " -> OK" << endl;
            mutex_cout.unlock();
            connack_value = true;
            ready = true;
            cond_var.notify_one();
            break;
        }
        case 1:{ // LOGIN ERROR
            mutex_cout.lock();
            cout << "From Client\tMessage: CONNACK\tto: "<< usuario_pass.username << " -> ERROR" << endl;
            mutex_cout.unlock();
            connack_value = false;
            ready = true;
            cond_var.notify_one();
            break;}
    }
}

void SimPublisher::recvPublishMsg(PublishMsg *M) const{
    mutex_cout.lock();
    cout << "From Client\tMessage: PUBLISH\tto: "<< usuario_pass.username <<"\t" << "Topic " << M->getTopic() << ": " << M->getValue() << endl;
    mutex_cout.unlock();
    return;
}
