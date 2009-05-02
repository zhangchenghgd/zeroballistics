
include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/network/src) 


set(rankingSources
src/RankingClientLogon.cpp
src/RankingMatchEvents.cpp
src/RankingMatchEventsSoccer.cpp
src/RankingRegisterMatch.cpp
src/RankingStatisticsSoccer.cpp
)


add_library(ranking ${rankingSources} )

