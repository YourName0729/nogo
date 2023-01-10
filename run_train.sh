while true
do
    ./nogo --total=1 --black="name=mcts balancing=1 thread=1 c=0.3 k=100 load=weight.bin save=weight.bin alpha=0.1" --white="name=mcts thread=1 c=0.1 k=100 time=10 skip=1"
    ./nogo --total=1 --white="name=mcts balancing=1 thread=1 c=0.3 k=100 load=weight.bin save=weight.bin alpha=0.1" --black="name=mcts thread=1 c=0.1 k=100 time=10 skip=1"
done