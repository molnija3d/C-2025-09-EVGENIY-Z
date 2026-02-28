valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.txt --errors-for-leak-kinds=all --num-callers=41 ./2048 
