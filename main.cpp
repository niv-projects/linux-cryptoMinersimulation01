#include <iostream>
#include "zlib.h"
#include <pthread.h>
#include <list>

#define assert_if(errNum, location) if (errNum !=0) {printf("ERROR: %m in %s\n", location); exit(EXIT_FAILURE);}

#define num_of_miners 4
#define dummy_miner_id 5

typedef struct {
    int height;        // Incremental ID of the block in the chain
    int timestamp;    // Time of the mine in seconds since epoch
    unsigned int hash;        // Current block hash value
    unsigned int prev_hash;    // Hash value of the previous block
    int difficulty;    // Amount of preceding zeros in the hash
    int nonce;        // Incremental integer to change the hash value
    int relayed_by;    // Miner ID
} BLOCK_T;

bool blockIsValid(unsigned int checksum, const std::list<BLOCK_T> &blockchain, const BLOCK_T &minedBlock);

int difficulty = 0;
pthread_mutex_t blockLock;
pthread_cond_t newBlockToMine;
pthread_cond_t blockWasMined;
BLOCK_T blockToCheck{};
BLOCK_T currentBlockToMine{};
bool boolBlockWasMined = false;
bool boolNewBlockInChain = false;

unsigned int calculateChecksum(const BLOCK_T &block) { //calculates checksum based on block's fields
    //initialize crc
    uLong crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.height), sizeof(block.height));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.timestamp), sizeof(block.timestamp));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.prev_hash), sizeof(block.prev_hash));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.nonce), sizeof(block.nonce));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.relayed_by), sizeof(block.relayed_by));

    return crc;
}

bool
validateDifficulty(unsigned int checksum) { //validates given checksum based on difficulty (number of leading zeroes)
    int numOfLeadingZeroes = 0;

    //count number of leading zeroes
    for (int i = 31; i >= 0; i--) {
        //shift right i bits AND 1
        if ((checksum >> i) & 1)
            break;
        numOfLeadingZeroes++;
    }
    //valid checksum
    if (numOfLeadingZeroes >= difficulty)
        return true;

    return false;
}

void *dummy_miner_thread_func(void *minerID) {
    int i = 0;

    //create dummy block
    BLOCK_T dummyBlock;
    dummyBlock.height = -1;
    dummyBlock.nonce = 0;
    dummyBlock.relayed_by = *(int *) minerID;
    dummyBlock.difficulty = difficulty;
    dummyBlock.prev_hash = 0xFFFFFFFF;
    dummyBlock.hash = calculateChecksum(dummyBlock);

    while (true) {
        pthread_mutex_lock(&blockLock);

        //calculate different checksum every 2nd iteration
        //for potential different errors by server
        if (i % 2 != 0) {
            dummyBlock.nonce++;
            dummyBlock.timestamp = static_cast<int>(time(nullptr));
            dummyBlock.hash = calculateChecksum(dummyBlock);
        }
            //update timestamp
            //causes error with server as it calculates a different checksum
        else
            dummyBlock.timestamp = static_cast<int>(time(nullptr));

        //signal the server with mined dummy block
        std::printf("Miner #%d: Mined a new block #%d, with the hash 0x%x\n", dummyBlock.relayed_by, dummyBlock.height,
                    dummyBlock.hash);
        blockToCheck = dummyBlock;
        boolBlockWasMined = true;
        pthread_cond_signal(&blockWasMined);
        pthread_mutex_unlock(&blockLock);
        i++;
        sleep(1);
    }

    return nullptr;

}

void *miner_thread_func(void *minerID) {
    int currentMinerID = *(int *) minerID;

    while (true) {
        pthread_mutex_lock(&blockLock);

        //wait for signal from server thread
        while (!boolNewBlockInChain)
            pthread_cond_wait(&newBlockToMine, &blockLock);

        pthread_mutex_unlock(&blockLock);

        //copy block to mine data
        BLOCK_T block = currentBlockToMine;
        block.relayed_by = currentMinerID;
        block.nonce = 0;

        //check if new block was appended
        //if so, go to beginning of loop and process again
        if (block.height != currentBlockToMine.height)
            continue;

        //set to false after processing new block
        boolNewBlockInChain = false;

        //loop and calculate hash whilst incrementing nonce
        while (true) {

            block.nonce++;
            block.timestamp = static_cast<int>(time(nullptr));
            block.hash = calculateChecksum(block);

            //check if hash and block are valid and signal server to validate
            if (validateDifficulty(block.hash)) {
                pthread_mutex_lock(&blockLock);
                std::printf("Miner #%d: Mined a new block #%d, with the hash 0x%x\n", block.relayed_by, block.height,
                            block.hash);
                blockToCheck = block;
                boolBlockWasMined = true;
                pthread_cond_signal(&blockWasMined);
                pthread_mutex_unlock(&blockLock);
            }
            //current block passed validation and is no longer the recent
            if (boolBlockWasMined)
                break;
        }
    }

    return nullptr;
}

void *server_thread_func(void *arg) {
    //set server thread priority to max as soon as it starts
    struct sched_param maxPriority = {sched_get_priority_max(SCHED_RR)};
    int res = pthread_setschedparam(pthread_self(), SCHED_RR, &maxPriority);
    assert_if(res, "server_thread_func setschedparam");

    //initialize server thread data
    pthread_mutex_lock(&blockLock);
    std::list<BLOCK_T> blockchain;
    //create genesis block
    BLOCK_T genesisBlock{};
    genesisBlock.difficulty = difficulty;
    genesisBlock.prev_hash = 0;
    genesisBlock.height = 0;
    genesisBlock.hash = 0xAAAAAAAA;
    genesisBlock.timestamp = static_cast<int>(time(nullptr));
    genesisBlock.relayed_by = -1;
    genesisBlock.nonce = 0;

    //add genesis block to blockchain
    blockchain.push_front(genesisBlock);

    //create new block based on genesis block
    BLOCK_T newBlock;
    newBlock.prev_hash = genesisBlock.hash;
    newBlock.height = (int) blockchain.size();
    newBlock.difficulty = difficulty;

    //assign new block to current block to mine
    currentBlockToMine = newBlock;
    boolNewBlockInChain = true;

    //broadcast new genesis block
    pthread_cond_broadcast(&newBlockToMine);
    pthread_mutex_unlock(&blockLock);

    while (true) {
        pthread_mutex_lock(&blockLock);

        //wait for signal from some miner
        while (!boolBlockWasMined)
            pthread_cond_wait(&blockWasMined, &blockLock);

        //get block mined from miner and calculate checksum
        BLOCK_T minedBlock = blockToCheck;
        unsigned int checksum = calculateChecksum(minedBlock);

        //validate block
        if (blockIsValid(checksum, blockchain, minedBlock)) {
            std::printf("Server: New block added by %d, attributes: ", minedBlock.relayed_by);
            std::printf("height(%d), timestamp (%d), hash(0x%x), ", minedBlock.height, minedBlock.timestamp,
                        minedBlock.hash);
            std::printf("prev_hash(0x%x), difficulty(%d), nonce(%d)\n", minedBlock.prev_hash, difficulty,
                        minedBlock.nonce);

            //add mined block to blockchain
            blockchain.push_front(minedBlock);

            //reset and update new block to mine with data based on previously mined block data
            newBlock = {};
            newBlock.prev_hash = minedBlock.hash;
            newBlock.height = (int) blockchain.size();
            newBlock.difficulty = difficulty;
            currentBlockToMine = newBlock;
        }
        //invalid block mined

        //reset booleans and broadcast to miners to mine block
        //*only when thread is signalled
        boolBlockWasMined = false;
        boolNewBlockInChain = true;
        pthread_cond_broadcast(&newBlockToMine);
        pthread_mutex_unlock(&blockLock);
    }

    return nullptr;
}

bool blockIsValid(unsigned int checksum, const std::list<BLOCK_T> &blockchain,
                  const BLOCK_T &minedBlock) { //validate mined block

    //validate difficulty
    if (!validateDifficulty(checksum)) {
        std::printf("Server: Miner #%d provided bad hash (0x%x) for block.\n",
                    minedBlock.relayed_by, minedBlock.hash);
        return false;
    }

    //validate checksum
    if (checksum != minedBlock.hash) {
        std::printf("Server: Miner #%d provided hash (0x%x) but server calculated (0x%x).\n",
                    minedBlock.relayed_by,minedBlock.hash, checksum);
        return false;
    }

    //validate height and prev_hash
    //if ((int) blockchain.size() > 1) {
    if (minedBlock.prev_hash != blockchain.front().hash || minedBlock.height != currentBlockToMine.height) {
        std::printf("Server: Miner #%d provided incorrect prev_hash (0x%x), does not reference most recent block in blockchain (0x%x).\n",
                    minedBlock.relayed_by, minedBlock.prev_hash, blockchain.front().hash);
        return false;
        }
    //}

    return true;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "ERROR: No input parameter (difficulty) given.\nUsage: ./program.out X\n *X must be a number between 0 and 32.\n";
        return 1;
    }
    //try processing input parameter (difficulty)
    try {
        difficulty = std::stoi(argv[1]);

        if (difficulty > 31 || difficulty < 0) {
            throw std::out_of_range("Parameter out of range.\n");
        }
    }
    catch (std::invalid_argument const &e) {
        std::cout << "ERROR: Invalid input parameter (must be a number).\n";
        return 1;
    }
    catch (std::out_of_range const &e) {
        std::cout << "ERROR: Input parameter out of range (must be between 0 and 31).\n";
        return 1;
    }

    pthread_t serverThread;
    pthread_t minerThreads[4];
    pthread_t dummyMiner;
    int minersIDs[num_of_miners];
    int dummyMinerID = dummy_miner_id;
    //initialize mutex and cond
    pthread_mutex_init(&blockLock, nullptr);
    pthread_cond_init(&newBlockToMine, nullptr);
    pthread_cond_init(&blockWasMined, nullptr);

    //create server and miners threads and join
    pthread_create(&serverThread, nullptr, server_thread_func, nullptr);
    for (int i = 0; i < 4; i++) {
        minersIDs[i] = i + 1;
        pthread_create(&minerThreads[i], nullptr, miner_thread_func, (void *) &minersIDs[i]);
    }
    pthread_create(&dummyMiner, nullptr, dummy_miner_thread_func, (void *) &dummyMinerID);

    pthread_join(serverThread, nullptr);
    for (int i = 0; i < 4; i++) {
        pthread_join(minerThreads[i], nullptr);
    }
    pthread_join(dummyMiner, nullptr);

    //destroy mutex and conditions
    pthread_mutex_destroy(&blockLock);
    pthread_cond_destroy(&newBlockToMine);
    pthread_cond_destroy(&blockWasMined);

    return 0;
}
