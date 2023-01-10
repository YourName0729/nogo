./nogo --total=5 --black="name=mcts thread=1 c=0.1" --white="name=mcts thread=4 c=0.3"
./nogo --total=2 --black="name=mcts thread_size=1 c=0.3 k=100 time=300 stat=stat.txt" --white="name=mcts thread_size=1 c=0.3 k=100 time=300 skip=1"
# ./nogo --total=1 --black="name=mcts c=2" --white="name=mcts c=1.5"