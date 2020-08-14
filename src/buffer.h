#include "packet.h"


typedef struct Node{
    packet p;
    struct Node *next;
} Node;

typedef struct BlockRep {
    int size;
    int amt_dup;
    Node *head;
} BlockRep;

typedef BlockRep *Block;


Block newBlock();
void dropBlock(Block);
Block BufferWrite(Block, packet);
packet BufferRead(Block);
packet BufferRead_Seq(Block, int);
Block CreateFromFile(char*, int, int);
bool inBuffer(Block, packet);
