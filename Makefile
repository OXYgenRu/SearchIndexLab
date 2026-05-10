CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g

OBJ_SHARED = posting.o avl/avl.o rbtree/rbtree.o btree/btree.o \
             index/index.o index/search.o

COMMON_LIBS = lab3/list/generic.o lab3/vector/generic.o lab4/hash_table/generic.o


.PHONY: all app u_tests test clean

all: app u_tests

app: $(OBJ_SHARED) $(COMMON_LIBS) main.o
	$(CC) $(CFLAGS) -o app $(OBJ_SHARED) $(COMMON_LIBS) main.o

test_avl: posting.o avl/avl.o avl/tests.o
	$(CC) $(CFLAGS) -o test_avl posting.o avl/avl.o avl/tests.o $(COMMON_LIBS)

test_rb: posting.o rbtree/rbtree.o rbtree/tests.o
	$(CC) $(CFLAGS) -o test_rb posting.o rbtree/rbtree.o rbtree/tests.o $(COMMON_LIBS)

test_btree: posting.o btree/btree.o btree/tests.o
	$(CC) $(CFLAGS) -o test_btree posting.o btree/btree.o btree/tests.o $(COMMON_LIBS)

u_tests: test_avl test_rb test_btree
	./test_avl
	./test_rb
	./test_btree

test: app
	@echo "=== E2E: preprocessing ==="
	mkdir -p data/test
	python3 preprocess.py \
		--input  data/test/Questions.csv \
		--output data/test/docs.jsonl
	@echo "=== E2E: indexing ==="
	./app index --type=avl   --data=data/test/docs.jsonl --index=data/test/idx_avl.txt
	./app index --type=rb    --data=data/test/docs.jsonl --index=data/test/idx_rb.txt
	./app index --type=btree --data=data/test/docs.jsonl --index=data/test/idx_btree.txt
	@echo "=== E2E: searching ==="
	./app search --type=avl   --index=data/test/idx_avl.txt   --json "python list"
	./app search --type=rb    --index=data/test/idx_rb.txt    --json "python list"
	./app search --type=btree --index=data/test/idx_btree.txt --json "python list"
	@echo "=== E2E OK ==="


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f app test_avl test_rb test_btree
	rm -f *.o avl/*.o rbtree/*.o btree/*.o index/*.o
	rm -f data/index_*.txt data/test/docs.jsonl data/test/idx_*.txt
	rm -f lab3/list/*.o lab3/vector/*.o lab4/hash_table/*.o