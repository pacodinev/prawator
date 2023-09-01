#!/bin/bash

# arguments: binary large/small worker_cnt optional_seed
function runBench {
    local pbin="$1"
    local pworkers="$2"
    if [ "$3" = "small" ]; then
        local size="--height 800 --width 400 --itercnt 5000"
    elif [ "$3" = "large" ]; then
        local size="--height 4000 --width 2000 --itercnt 500"
    fi
    local pht=""
    if [ $pworkers -le 16 ]; then
        local pht="--disable-ht"
    fi
    local seed=""
    if [ -n "$4" ]; then
        local seed="--seed $4"
    fi

    command="$pbin $size --workers $pworkers $pht $seed --benchmark"
    echo ${command} :
    # eval "${command}"
}

# arguments: binary
function runBenchForBin {
    for size in small large
    do
        for workers in 1 2 4 6 8 12 16 20 24 28 32
        do
            echo "-----------------BEGIN HERE---------------------"
            echo "Size:" $size "; Workers:" $workers
            for seed in 2688 6396 7852 9892 6308 2698 6402
            do
                for it in {1..2}
                do
                    runBench $1 $workers $size $seed
                done
            done
            echo "-----------------END HERE-----------------------"
        done
    done
}

runBenchForBin ~/parwatorBinCN 2>&1 | tee parwatorBinCN.log
runBenchForBin ~/parwatorBinC  2>&1 | tee parwatorBinC.log
runBenchForBin ~/parwatorBin   2>&1 | tee parwatorBin.log
