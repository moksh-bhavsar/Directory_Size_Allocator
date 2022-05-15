#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <filesystem>
#include <pthread.h>
#include <atomic>

// global variables for multithreading
std::atomic<int> TOTAL_DISK_SIZE(0);
std::atomic<int> ACTUAL_DISK_SIZE(0);

/**
 * @brief A struct to pass data to thread handler function. It contains the path to directory and 
 * the recursion limit user wants.
 * 
 */
struct individual{
    char* pathToDirectory;
    int* recursionLimit;
};

/**
 * @brief the function outputs the file name (with full absolute path), the number of 
 * block it uses and its actual size.
 * 
 * @param pathToDirectory: path to the directory 
 */
void displayInfo(char** pathToDirectory){
    char * dir;
    struct stat buff;

    dir = pathToDirectory[1];

    // opening a fd, making sure it is a directory with O_DIRECTORY flag
    int fin = open(dir, O_DIRECTORY);
    if (fin < 0){
        std::cerr << "Cannot open the directory" << std::endl;
        exit(EXIT_FAILURE);
    }

    // creating a iterator that iterates over the files in the directory.
    auto it = std::filesystem::directory_iterator{dir};

    for (std::filesystem::directory_entry dir: it){
        if (dir.is_regular_file()){
            fstat(open(dir.path().c_str(), O_RDONLY), &buff);
            std::cout << "File: " << dir.path() << std::endl;
            std::cout << "Total size: " << (buff.st_blksize)* (buff.st_blocks) << std::endl;
            std::cout << "Actual size: " << buff.st_size << std::endl;
            std::cout << std::endl;
        }
    }

    close(fin);
}

/**
 * @brief the function similar to the displayInfo function, but it asks user for
 * recursion limit and does not calculate sizes for files after it reaches the
 * limit mentioned
 * 
 * @param pathToDirectory: path to the directory 
 */
void displayInfo2(char** pathToDirectory){
    // variables to find the sizes for the directory
    int actualDiskSize, totalDiskSize = 0;

    char * dir;
    struct stat buff;

    const int* recursionLimit = (const int*) pathToDirectory[1];
    dir = pathToDirectory[2];

    // opening a fd, making sure it is a directory with O_DIRECTORY flag
    int fin = open(dir, O_DIRECTORY);
    if (fin < 0){
        std::cerr << "Cannot open the directory" << std::endl;
        exit(EXIT_FAILURE);
    }

    // creating a iterator that iterates over the files in the directory.
    auto it = std::filesystem::recursive_directory_iterator{dir};
    
    for (std::filesystem::directory_entry dir: it){
        if (it.depth() <= *recursionLimit){
            if (dir.is_regular_file()){
                fstat(open(dir.path().c_str(), O_RDONLY), &buff);
                totalDiskSize += (buff.st_blksize)* (buff.st_blocks);
                actualDiskSize += buff.st_size;
            }
        }
    }

    close(fin);

    std::cout << "Total Disk size: " << totalDiskSize << std::endl;
    std::cout << "Actual Disk size: " << actualDiskSize << std::endl;
    std::cout << std::endl;

}

/**
 * @brief Function to get disk size, both actual and total. This is a thread handler function.
 * It modifies both directory's disk size as well as disk size for all directories mentioned.
 * 
 * @param pathToDirectory- struct that contains path to directory and the recursion Limit
 */
void* displayInfo3(void* pathToDirectory){
    // variables for disk size of the directory
    int totalDiskSize, actualDiskSize = 0;
    struct stat buff;
    individual* dir = (individual *)pathToDirectory;

    int fin = open(dir->pathToDirectory, O_DIRECTORY);
    if (fin < 0){
        std::cerr << "Cannot open the directory" << std::endl;
        exit(EXIT_FAILURE);
    }

    // creating a iterator that iterates over the files in the directory.
    auto it = std::filesystem::recursive_directory_iterator{dir->pathToDirectory};
    int* maxDepth = dir->recursionLimit;
    
    for (std::filesystem::directory_entry dir: it){
        if (it.depth() <= *maxDepth){
            if (dir.is_regular_file()){
                fstat(open(dir.path().c_str(), O_RDONLY), &buff);

                // updating the directory sizes and overall sizes
                totalDiskSize += (buff.st_blksize)* (buff.st_blocks);
                actualDiskSize += buff.st_size;
                TOTAL_DISK_SIZE = TOTAL_DISK_SIZE.load() + (buff.st_blksize * buff.st_blocks);
                ACTUAL_DISK_SIZE = ACTUAL_DISK_SIZE.load() + buff.st_size;
            }
        }
    }
    close(fin);

    std::cout << "File/Dir: " << dir->pathToDirectory << std::endl;
    std::cout << "Total Disk size: " << totalDiskSize << std::endl;
    std::cout << "Actual Disk size: " << actualDiskSize << std::endl; 
    std::cout << std::endl;

    return NULL;
}

int main(int argc, char** argv){
    // checking the arg count and using the revelant info
    if (argc == 2){
        displayInfo(argv);
    }else if (argc == 3){
        displayInfo2(argv);
    }
    else if (argc > 3){
        int numThreads = argc - 2; // -2 as we have the exe file and recursion limit as arguments

        // creating array of threads
        pthread_t* threads = new pthread_t[numThreads];

        // creating pthreads
        int rt;
        for (int i = 0; i<numThreads; i++){
            individual* data = new individual();
            data->pathToDirectory = argv[i+2];
            data->recursionLimit = (int*) argv[1];
            rt = pthread_create(&threads[i], NULL, displayInfo3, (void*) data);
            if (rt != 0){
                printf("Error in creating a thread");
                exit(EXIT_FAILURE);
            }
        }

        // joining pthreads to wait until all of them are done
        for (int i =0; i< numThreads; i++){
            rt = pthread_join(threads[i], NULL);
            if (rt != 0){
                printf("Threads not able to join");
                exit(EXIT_FAILURE);
            }
        }

        printf("\nThe stats for the given files:\n");
        std::cout << "Total Disk Size: " << TOTAL_DISK_SIZE.load() << std::endl;
        std::cout << "Actual Disk Size: " << ACTUAL_DISK_SIZE.load() << std::endl;
        

    }
    else{
        printf("Not enough arguments\n");
        exit(EXIT_FAILURE);
    }
}