#include <eosiolib/eosio.hpp>
#include <eosiolib/public_key.hpp>
#include <eosiolib/types.hpp>
#include <eosiolib/asset.hpp>
#include <string>
#include <eosiolib/currency.hpp>
#define MAX_CORONATION_TIME 300

using namespace eosio;

class eosbaozi : public eosio::contract
{
  public:
    eosbaozi(account_name self)
        : contract(self),
        accrecords(_self, _self),
        gametables(_self, _self)
        
    {
    }

   typedef enum order_status
    {
        LOCKING = 0,        // 有人下注，不允许撤销桌子
        OPEN,            // 没人下注
    } order_status;

    struct player
    {
        player(){};
        player(account_name name, asset bet, vector<uint16_t> poker )
            : name(name), bet(bet), poker(poker){};
        account_name name;
        asset bet;
        vector<uint16_t> poker;

        EOSLIB_SERIALIZE(player, (name)(bet)(poker))
    };

    struct game
    {
        game(){};
        game(block_id_type result, vector<player> players, vector<uint8_t> bankpoker )
            : result(result), players(players), bankpoker(bankpoker){};
            
        block_id_type result;
        vector<player> players;
        vector<uint8_t> bankpoker;

        EOSLIB_SERIALIZE(game, (result)(players)(bankpoker))
    };

    //@abi table gametables i64
    struct gametable
    {
        account_name banker; 
        vector<player> players;
        asset stake;
        uint16_t status;
        time starttime;
        uint64_t round;
        game pregame;

        uint64_t primary_key() const { return banker; }
        // need to serialize this, otherwise saving it in the data base does not work
        // Runtime Error Processing WASM
        EOSLIB_SERIALIZE(gametable, (banker)(players)(stake)(status)(starttime)(round)(pregame))
    };

    //@abi table accrecords i64
    struct account_record
    {
        account_name player; 
        asset balance;
        account_name inviter;

        uint64_t primary_key() const { return player; }
        // need to serialize this, otherwise saving it in the data base does not work
        // Runtime Error Processing WASM
        EOSLIB_SERIALIZE(account_record, (player)(balance)(inviter))
    };    

    //@abi action draw
    struct draw
    {
        draw(){};
        // action must have a field as of now
        account_name name;
        EOSLIB_SERIALIZE(draw, (name))
    };

    //@abi action withdraw
    struct withdraw
    {
        withdraw(){};
        // action must have a field as of now
        account_name name;
        EOSLIB_SERIALIZE(withdraw, (name))
    };

    //@abi action drop
    struct drop
    {
        drop(){};
        // action must have a field as of now
        account_name name;
        EOSLIB_SERIALIZE(drop, (name))
    };
    // the first argument of multi_index must be the name of the table
    // in the ABI!
    typedef eosio::multi_index<N(accrecords), account_record> account_records_db;
    typedef eosio::multi_index<N(gametables), gametable> gametables_db;

    void onTransfer(const eosio::currency::transfer& transfer);
    void draw(account_name banker);
    void withdraw(account_name player);
    void drop(account_name banker);
    void apply( account_name contract, account_name act );

  private:
    vector<uint8_t> deck = {1,2,3,4,5,6,7,8,9,10,1,2,3,4,5,6,7,8,9,10,1,2,3,4,5,6,7,8,9,10,1,2,3,4,5,6,7,8,9,10};
    account_records_db accrecords;
    gametables_db gametables;
    void addbalance(account_name player, asset balance);
    void addinviter(account_name player, account_name inviter);
    void generaterandom(checksum256 &result, uint64_t round);
    uint8_t dealpoker(vector<uint8_t> &poker,checksum256 result, int turn);
    void create(account_name banker, asset stake);
    void bet(account_name banker,account_name player,asset bet);
};