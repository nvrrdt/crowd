#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <openssl/evp.h>

#include "merkle_tree.hpp"

#include <fstream>
#include "json.hpp"

#include <chrono>
#include <ctime>
#include <unistd.h>

#include <math.h>
#include <memory>
#include <stack>

using namespace crowd;

/**
 * a block consists of a timestamp hashed just before the root hash and a merkle tree of users
 * or a list of alfabetically sorted users and a root hash
 * each user is hashed email concatenatenated with a hashed password
 * when logging in the hashed email and hashed password must be found in the verified blockchain
 * logging in with email and password and blockchain id, the last one is for trying to make a private blockchain
 * the timestamp is the start of a timeframe of 1 hour
 */



void merkle_tree::create_user(string email, string password)
{
    string email_hashed, password_hashed, user_conc, user_hashed;

    if (merkle_tree::create_hash(email, email_hashed) &&
        merkle_tree::create_hash(password, password_hashed) == true)
    {
        // Add new user's credentials to pool:
        merkle_tree::save_new_user(email_hashed, password_hashed);
    }
}

bool merkle_tree::create_hash(const string& unhashed, string& hashed)
{
    bool success = false;

    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if(context != NULL)
    {
        if(EVP_DigestInit_ex(context, EVP_sha256(), NULL))
        {
            if(EVP_DigestUpdate(context, unhashed.c_str(), unhashed.length()))
            {
                unsigned char hash[EVP_MAX_MD_SIZE];
                unsigned int lengthOfHash = 0;

                if(EVP_DigestFinal_ex(context, hash, &lengthOfHash))
                {
                    std::stringstream ss;
                    for(unsigned int i = 0; i < lengthOfHash; ++i)
                    {
                        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
                    }

                    hashed = ss.str();
                    success = true;
                }
            }
        }

        EVP_MD_CTX_free(context);
    }

    return success;
}

void merkle_tree::save_new_user(string& hashed_email_new_user, string& hashed_password_new_user)
{
    ifstream ifile("../new_users_pool.json", ios::in);

    nlohmann::json j;

    if (merkle_tree::is_empty(ifile))
    {
        ofstream ofile("../new_users_pool.json", ios::out);
        
        ofile.clear();

        nlohmann::json i;
        i.push_back(hashed_email_new_user);
        i.push_back(hashed_password_new_user);

        j.push_back(i);

        ofile << j;
    }
    else
    {
        ofstream ofile("../new_users_pool.json", ios::out);

        ifile >> j;
        ofile.clear();

        nlohmann::json i;

        i.push_back(hashed_email_new_user);
        i.push_back(hashed_password_new_user);
    
        j += i;

        ofile << j;
    }
}

bool merkle_tree::is_empty(std::ifstream& pFile)
{
    return pFile.peek() == std::ifstream::traits_type::eof();
}

void merkle_tree::prep_block_creation()
{
    merkle_tree::two_hours_timer();

    // parse new_users_pool.json
    ifstream file("../new_users_pool.json");
    nlohmann::json j;

    file >> j;
    
    shared_ptr<stack<string>> s_shptr = make_shared<stack<string>>();

    for (auto& element : j) {
        std::cout << std::setw(4) << element << '\n';

        // sort email_hashed and password_hashed alphabetically for consistency in concatenating these two hashes
        string email_hashed, password_hashed, user_conc, user_hashed;
        // TODO: the user's of the block, only hashed email and hashed password, need also to be stored in a json with block number
        // but there's block arithmetic needed when the winning block is smaller and bigger than this one calculated here ...
        email_hashed = element[0];
        password_hashed = element[1];
        if (email_hashed <= password_hashed)
        {
            user_conc = email_hashed + password_hashed;
        }
        else 
        {
            user_conc = password_hashed + email_hashed;
        }

        if (merkle_tree::create_hash(user_conc, user_hashed) == true)
        {
            std::cout << "root: " << string(user_hashed) << endl;

            s_shptr->push(user_hashed);
        }
    }

    s_shptr = merkle_tree::calculate_root_hash(s_shptr);

    std::cout << "root hash block: " << s_shptr->top() << endl;

    merkle_tree::create_block();

    // TODO: setup the block and and add to the blockchain, see the text in the beginning of this file for missing information
}

int merkle_tree::two_hours_timer()
{
    using namespace std::chrono;
    /* TODO: remove this comment in production
    while (1)
    {
        system_clock::time_point now = system_clock::now();

        time_t tt = system_clock::to_time_t(now);
        tm utc_tm = *gmtime(&tt);
        
        if (utc_tm.tm_sec % 60 == 0 &&
            utc_tm.tm_min % 60 == 0 &&
            utc_tm.tm_hour % 2 == 0) // == 2 hours
        {
            std::cout << utc_tm.tm_hour << ":";
            std::cout << utc_tm.tm_min << endl;

            break;
        }
    }
    */
    sleep(1);

    return 0;
}

shared_ptr<stack<string>> merkle_tree::calculate_root_hash(shared_ptr<stack<string>> s_shptr)
{
    size_t n; // 2^n

    // calculate the next 2^n above stacksize
    for (size_t i = 0; i < s_shptr->size(); i++)
    {
        if (pow (2, i) >= s_shptr->size())
        {
            n = pow (2, i);
            break;
        }
    }

    // add 0's to the stack till a size of 2^n
    size_t current_stack_size = s_shptr->size();
    std::cout << n << " " << current_stack_size << endl;
    for (size_t i = 0; i < (n - current_stack_size); i++)
    {
        string zero = "zero", zero_hashed;
        if (merkle_tree::create_hash(zero, zero_hashed) == true)
        {
            s_shptr->push(zero_hashed);
        }
    }

    return merkle_tree::pop_two_and_hash(s_shptr);
}

shared_ptr<stack<string>> merkle_tree::pop_two_and_hash(shared_ptr<stack<string>> s_shptr)
{
    string uneven, even;
    shared_ptr<stack<string>> s1_shptr = make_shared<stack<string>>();

    if (s_shptr->size() <= 1)
    {
        return s_shptr; // only root_hash_block is in stack s_shptr
    }
    else
    {
        while (!s_shptr->empty())
        {
            uneven = s_shptr->top(); // left!
            s_shptr->pop();

            even = s_shptr->top(); // right!
            s_shptr->pop();

            string parent_conc = uneven + even, parent_hashed;

            if (merkle_tree::create_hash(parent_conc, parent_hashed) == true)
            {
                s1_shptr->push(parent_hashed);
            }
        }

        return merkle_tree::pop_two_and_hash(s1_shptr);
    }
}

void merkle_tree::create_block()
{
    /**
     * create file with timeframe number in blockchain folder: 'block00000000.json'
     * put in timeframe number, the hashes (not root hashes!!) from the users (data), the block's root hash and the previous block's root hash and the hash from the chosen one
     * later on: verification of the latest block's root hash
     * if no new user is added in a timeframe, then the counter goes up, without block creation, so the files don't necessary need following nbumbers
     * 
     * for genesis block also a timestamp needs to be added, maybe some news fact can also be shared, perhaps stored in prev_hash
     */

    cout << "goed" << endl;
}