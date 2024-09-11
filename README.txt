FINAL GRADE: 95
MtaCoin: Multi-threaded Blockchain Mining Simulation

This project implements a multi-threaded cryptocurrency blockchain mining simulation in C++. It simulates a simplified version of blockchain mining with 4 miner threads and 1 server thread, as per the assignment requirements. The miners calculate hashes for blocks, while the server verifies and appends valid blocks to the blockchain.

Features:
Multithreaded Mining: Multiple miner threads compete to mine blocks and submit them to the server for validation.

Server Validation: The server verifies the mined blocks and ensures they adhere to the specified proof-of-work difficulty.

Blockchain Management: The server maintains a list of blocks (the blockchain) and ensures that only valid blocks are appended.

Difficulty Setting: The difficulty of the proof-of-work is adjustable by passing a command-line argument to the program.

Error Handling: Includes a dummy miner that deliberately mines invalid blocks to test the server's handling of faulty inputs.

how to use ?

Prerequisites:

Ensure you have pthread and zlib installed in your development environment. 

Building the Project using CMake:

1.Clone the repository:
command:
git clone https://github.com/niv-projects/linux-cryptoMinersimulation01.git

2.Navigate to the project directory:
command:
cd miners

3.Create a build directory and compile the project:
command:

mkdir build
cd build
cmake ..
make

Running the Program:
Once built, you can run the program with the desired difficulty level (between 0 and 31):

./miners.out <difficulty>

For example:

./miners.out 5
