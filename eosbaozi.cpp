#include "./eosbaozi.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>        // for SEND_INLINE_ACTION
#include <cmath> // for pow
#include <boost/algorithm/string.hpp> // for split
#include <eosiolib/transaction.h>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <utility>

#define CORE_SYMBOL S(4,EOS) // MainNet + JungleTestNet use EOS

using namespace eosio;
using namespace std;

inline void splitMemo(std::vector<std::string> &results, std::string memo)
{
    auto end = memo.cend();
    auto start = memo.cbegin();

    for (auto it = memo.cbegin(); it != end; ++it)
    {
        if (*it == ';')
        {
            results.emplace_back(start, it);
            start = it + 1;
        }
    }
    if (start != end)
        results.emplace_back(start, end);
}

inline void sendreward(account_name from, account_name to, asset reward){
    action transfers(
        permission_level{from, N(active)},
        N(eosio.token), N(transfer),
        std::make_tuple(from, to, reward, std::string("")));
    transfers.send();
}

 bool greatersort(eosbaozi::player a, eosbaozi::player b){ 
     return ((a.poker[0]+a.poker[1]) > (b.poker[0]+b.poker[1]));
}

void eosbaozi::onTransfer(const currency::transfer &transfer)
{
    if (transfer.to != _self)
        return;

    eosio_assert(transfer.from != _self, "deployed contract may not take part in claiming the throne");

    // print("Transfer memo: ", transfer.memo.c_str());
    eosio_assert(transfer.quantity.symbol == CORE_SYMBOL, "must pay with EOS token");
    std::vector<std::string> results;
    // use custom split function as we save 20 KiB RAM this way
    // boost::split(results, transfer.memo, [](const char c) { return c == ';'; });
    splitMemo(results, transfer.memo);
    eosio_assert(results.size() >= 2 , "transfer memo needs two arguments separated by ';'");
    eosio_assert(results[0] == "eosbaozi" && (results[1] == "create" || results[1] == "bet"), "kingdom arguments failed the size requirements");

    if(results[1]=="create"){
        create(transfer.from, transfer.quantity);
    }else{
        eosio_assert(results.size() >= 3 , "transfer memo needs three arguments separated by ';'");
        bet(N(results[2]), transfer.from, transfer.quantity);
    }

    if(results.size() >= 4 && (results[3].length()>0)){
        addinviter(transfer.from, N(results[3]));
    }
}

void eosbaozi::addinviter(account_name player,account_name inviter){
    auto itr = accrecords.find(player);
        if(itr == accrecords.end()){
            accrecords.emplace(player,[&](auto &newplayer) {
            newplayer.player = player;
            newplayer.balance = asset(0,CORE_SYMBOL);
            newplayer.inviter = inviter;
            });
        }else{
            if(is_account(itr->inviter)){
                accrecords.modify( itr,  player, [&]( auto& oldplayer ) {
                oldplayer.inviter = inviter;
                });
            }
        }
    
    
}

void eosbaozi::create(account_name banker, asset stake)
{
    auto itr = gametables.find(banker);

    eosio_assert(itr == gametables.end(), "table is exist");

    auto beginitr = gametables.begin();
    int i = 0;
    for(;i<100;i++){
        if(beginitr!=gametables.end()){
            beginitr++;
        }
    }

    eosio_assert(i<100, "game table is too much");

    gametables.emplace(banker,[&](auto &gametable) {
        gametable.banker = banker;
        gametable.stake = stake;
        gametable.status = OPEN;
        gametable.round = 1;
    });
}

void eosbaozi::bet(account_name banker,account_name player, asset bet)
{
    auto itr = gametables.find(banker);

    eosio_assert(itr != gametables.end(), "table is not exist");
    eosio_assert(bet.amount < 100, "bet is too much");
    vector<eosbaozi::player> gameplayers = itr->players;
    eosio_assert(gameplayers.size() < 9, "player is too much");
    vector<uint16_t> poker;
    eosbaozi::player newplayer = eosbaozi::player(player,bet,poker);
    if(gameplayers.size()==0){
        gameplayers.emplace_back(newplayer);
        gametables.modify( itr,  banker, [&]( auto& gametable ) {
        gametable.players = gameplayers;
        gametable.status = LOCKING;
        gametable.starttime = now();
        });
    }else{
        gameplayers.emplace_back(newplayer);
        gametables.modify( itr,  banker, [&]( auto& gametable ) {
        gametable.players = gameplayers;
        gametable.status = LOCKING;
        });
    }
    if(itr->players.size()==9){
        draw(banker);
    }
}

void eosbaozi::draw(account_name banker)
{
    auto itr = gametables.find(banker);
    eosio_assert(itr != gametables.end(), "table is not exist");
    eosio_assert(itr->players.size() > 0 , "don't have player");
    eosio_assert(now() > itr->starttime + MAX_CORONATION_TIME, "max coronation time not reached yet");
    vector<uint8_t> poker = deck;
    vector<player> curplayers = itr->players;

    checksum256 result;
    generaterandom(result,itr->round);
    uint8_t bankpoker1 = dealpoker(poker,result,0);
    uint8_t bankpoker2 = dealpoker(poker,result,1);
    uint8_t bankpoker = bankpoker1+bankpoker2;
    vector<uint8_t> curbankpoker = {bankpoker1, bankpoker2};
    for(uint8_t playerturn = 1;playerturn<=curplayers.size();playerturn++){
        curplayers[playerturn-1].poker[0] = dealpoker(poker,result,playerturn*2);
        curplayers[playerturn-1].poker[1] = dealpoker(poker,result,playerturn*2+1);
    }

    sort(curplayers.begin(),curplayers.end(),greatersort);

    asset curstake = itr->stake;
    for(auto itp=curplayers.begin();itp != curplayers.end();++itp){
        curstake += curstake + itp->bet;
    }

    for(auto it=curplayers.begin();it != curplayers.end();++it){
        if(bankpoker < (it->poker[0]+it->poker[1])){
            if(curstake.amount>0){
                if(curstake>=it->bet){
                    addbalance(it->name,(it->bet)*2);
                    curstake -= (it->bet)*2;
                }else{
                    addbalance(it->name, (it->bet + curstake));
                    curstake = asset(0,CORE_SYMBOL);
                }
            }
        }
    }

    if(curstake.amount < 10){
        addbalance(banker, itr->stake);
        gametables.erase(itr);
    }else{
        vector<player> newplayers ;
        game pregame(result, curplayers, curbankpoker);
        gametables.modify(itr, banker, [&]( auto& gametable ){
        gametable.stake = curstake;
        gametable.pregame = pregame;
        gametable.status = OPEN;
        gametable.round += 1;
        gametable.starttime = now();
        gametable.players = newplayers;
        });
    }
    


}

uint8_t eosbaozi::dealpoker(vector<uint8_t> &poker,checksum256 result, int turn){
    uint8_t seat = result.hash[turn]%poker.size();
    uint8_t isdealpoker = poker[seat];
    poker.erase(poker.begin()+seat);
    return isdealpoker;

}

void eosbaozi::generaterandom(checksum256 &result, uint64_t round){
    static uint64_t seed = static_cast<uint64_t>((int)&result);
    
    auto mixedBlock = tapos_block_prefix() + tapos_block_num() + round + seed + current_time();
        
    seed += (mixedBlock >> 33);

    const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
    sha256((char *)mixedChar, sizeof(mixedChar), &result);

}

void eosbaozi::drop(account_name banker)
{
    require_auth(banker);
    auto itr = gametables.find(banker);
    eosio_assert(itr != gametables.end(), "table is not exist");
    eosio_assert(itr->status == OPEN, "table is LOCKING");

    addbalance(banker, itr->stake);
    gametables.erase(itr);
}

void eosbaozi::addbalance(account_name player, asset balance)
{
    auto itr = accrecords.find(player);
    if(itr == accrecords.end()){
        accrecords.emplace(player,[&](auto &newplayer) {
        newplayer.player = player;
        newplayer.balance = balance;
        });

    }else{
        accrecords.modify( itr,  player, [&]( auto& oldplayer ) {
        oldplayer.balance += balance;
        });
    }
    
}

void eosbaozi::withdraw(account_name player)
{
    require_auth(player);
    auto itr = accrecords.find(player);
    eosio_assert(itr != accrecords.end(), "player is not exist");

    action transferact(
            permission_level{_self, N(active)},
            N(eosio.token), N(transfer),
            std::make_tuple(_self, player, itr->balance, std::string("")));
    transferact.send();

    accrecords.modify( itr,  player, [&]( auto& oldplayer ) {
        oldplayer.balance = asset(0,CORE_SYMBOL);
    });

    

}

void eosbaozi::apply(account_name contract, account_name act)
{
    if (contract == N(eosio.token) && act == N(transfer))
    {
        onTransfer(unpack_action_data<currency::transfer>());
        return;
    }

    if (contract != _self)
        return;

    // needed for EOSIO_API macro
    auto &thiscontract = *this;
    switch (act)
    {
        // first argument is name of CPP class, not contract
        EOSIO_API(eosbaozi, (draw)(withdraw))
    };
}

// EOSIO_ABI only works for contract == this conract
// EOSIO_ABI(eosbaozi, (transfer))

extern "C"
{
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        eosbaozi baozi(receiver);
        baozi.apply(code, action);
        eosio_exit(0);
    }
}


