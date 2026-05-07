# Лабораторная работа №14 — AVL, Red-Black и B-tree: поисковый индекс

Три дерева заходят в бар. AVL заказывает два напитка и немедленно балансирует бокалы. Red-Black говорит: «Ладно, пусть стоит немного криво, главное — быстро налить». B-tree заказывает сразу пятнадцать и спрашивает: «А можно я займу целую страницу меню?»

Балансировка — это искусство компромисса: между строгостью и скоростью вставки, между памятью и кэш-дружелюбностью. В этой лабораторной вы реализуете все три структуры и проверите их на реальных данных — корпусе вопросов и ответов Stack Overflow.


# Команды
## Запись в команды происходит в форме
https://forms.gle/JFWrPSRiAca63EYt9

Короче, от 3 до 7 человек.
Можете делать в отдельном от орги репозитории, просто потом сменить ориджин на репу в орге и все смержите. Лаба либеральная, в целом, основная задача сделать так чтобы самим на гитхабе было не стыдно держать проект и чтобы ментору было весело смотреть.  
Структура примерная, отойдём от шаблонности. Просто реализуем поисковую систему с различными движками индексации.  
Хотите - запилите фронт, хотите бахните доп реализацию на расте, хотите стройте графики или бенчите это. В общем минимум - реализация структур, максимум - новый ElasticSearch.  

---

## Цель работы

Реализовать AVL-дерево, Red-Black Tree и B-tree. Сравнить их производительность на задаче построения инвертированного текстового индекса и поиска по нему.

---

## Датасет

Используется [Stack Overflow dataset](https://www.kaggle.com/datasets/stackoverflow/stacksample).

Скачайте и положите в папку `data/`:
```
lab14/
└── data/
    ├── Questions.csv
    ├── Answers.csv
    └── Tags.csv
```

---

## Теоретическая справка

### Инвертированный индекс

Инвертированный индекс — основа полнотекстового поиска. Вместо «документ → слова» он хранит «слово → документы»:

```
"python"  → [(doc_id=1, "How to use Python?"), (doc_id=5, "Python tips")]
"list"    → [(doc_id=1, ...), (doc_id=3, ...), (doc_id=7, ...)]
"sort"    → [(doc_id=3, ...), (doc_id=7, ...)]
```

Поиск по запросу `"python list sort"` — это пересечение трёх posting list'ов.

### Три дерева поиска

| Свойство | AVL | Red-Black | B-tree (порядок t) |
|---|---|---|---|
| Высота | ~1.44 log₂ n | ~2 log₂ n | log_t n |
| Поиск | O(log n) | O(log n) | O(t · log_t n) |
| Вставка | O(log n) | O(log n) | O(t · log_t n) |
| Ротации при вставке | ≤ 2 | ≤ 2 | 0 (сплит) |
| Кэш-дружелюбность | низкая | низкая | высокая |
| Применение | словари в памяти | ядро Linux, Java TreeMap | файловые системы, БД |

**AVL** — строгая балансировка: разница высот поддеревьев не превышает 1. Быстрее при поиске, медленнее при вставке.

**Red-Black** — мягкая балансировка: высота не превышает 2·log₂(n+1). Быстрее при вставке и удалении.

**B-tree** — ветвистое дерево для работы со страницами: каждый узел хранит от t-1 до 2t-1 ключей. Оптимален, когда данные не влезают в кэш процессора.

---

## Задание

| Задача | Сложность |
|---|---|
| 1. Реализация AVL, Red-Black, B-tree | Базовая |
| 2. Индексация и поиск | Базовая |
| 3. Эксперименты | Базовая |
| 4. Fuzzy Search | Hard |

---

## Задача 1: Реализация структур данных

Реализовать три структуры с нуля. Каждая должна поддерживать:
- вставку ключа (строка-терм)
- поиск по ключу
- хранение posting list'а (список `(document_id, title)`)

### Posting list (общий для всех деревьев)

Posting list — это `Vector` из lab3. Переиспользуйте `lab3/vector/generic.{h,c}` напрямую.

```c
#include "../lab3/vector/generic.h"

typedef struct {
    int  doc_id;
    char title[256];
} PostingEntry;

/* Создаёт пустой posting list (Vector с elem_size = sizeof(PostingEntry)) */
Vector* createPostingList(void);

/* Добавляет запись в конец списка */
void appendPosting(Vector* list, int doc_id, const char* title);

/* Возвращает глубокую копию списка */
Vector* clonePostingList(const Vector* list);
```

Для доступа к элементам:

```c
size_t n = list->size;
PostingEntry* e = getVectorItem(list, i);
printf("doc_id=%d title=%s\n", e->doc_id, e->title);
```

Освобождение — стандартным `vectorFree(list)`.

### AVL-дерево

```c
typedef struct AVLNode {
    char*           key;
    int             height;
    Vector*         postings;
    struct AVLNode* left;
    struct AVLNode* right;
} AVLNode;

typedef struct {
    AVLNode* root;
    int      size;
} AVLTree;

AVLTree* createAVLTree(void);
void     freeAVLTree(AVLTree* tree);

void    avlInsert(AVLTree* tree, const char* key, int doc_id, const char* title);
Vector* avlSearch(const AVLTree* tree, const char* key);
void    avlTraverse(const AVLTree* tree,
                    void (*visit)(const char* key, Vector* postings, void* ctx),
                    void* ctx);
```

### Red-Black Tree

```c
typedef enum { RB_RED, RB_BLACK } RBColor;

typedef struct RBNode {
    char*          key;
    RBColor        color;
    Vector*        postings;
    struct RBNode* left;
    struct RBNode* right;
    struct RBNode* parent;
} RBNode;

typedef struct {
    RBNode* root;
    RBNode* nil;   /* sentinel-узел (чёрный лист) */
    int     size;
} RBTree;

RBTree* createRBTree(void);
void    freeRBTree(RBTree* tree);

void    rbInsert(RBTree* tree, const char* key, int doc_id, const char* title);
Vector* rbSearch(const RBTree* tree, const char* key);
void    rbTraverse(const RBTree* tree,
                   void (*visit)(const char* key, Vector* postings, void* ctx),
                   void* ctx);
```

### B-tree

```c
#define BTREE_T        3   /* минимальная степень; узел хранит от t-1 до 2t-1 ключей */
#define BTREE_MAX_KEYS (2 * BTREE_T - 1)
#define BTREE_MAX_CH   (2 * BTREE_T)

typedef struct BTreeNode {
    char*             keys[BTREE_MAX_KEYS];
    Vector*           postings[BTREE_MAX_KEYS];
    struct BTreeNode* children[BTREE_MAX_CH];
    int               n;
    int               is_leaf;
} BTreeNode;

typedef struct {
    BTreeNode* root;
    int        size;
} BTree;

BTree*  createBTree(void);
void    freeBTree(BTree* tree);

void    btreeInsert(BTree* tree, const char* key, int doc_id, const char* title);
Vector* btreeSearch(const BTree* tree, const char* key);
void    btreeTraverse(const BTree* tree,
                      void (*visit)(const char* key, Vector* postings, void* ctx),
                      void* ctx);
```

---

## Задача 2: Индексация и поиск

### Шаг 1: Препроцессинг (Python, один раз)

Скрипт `preprocess.py` читает `Questions.csv`, приводит текст к нижнему регистру, убирает пунктуацию, разбивает на токены и сохраняет результат:

```bash
python preprocess.py --input data/Questions.csv --output data/processed/docs.jsonl
```

Формат `docs.jsonl` — по одной JSON-строке на документ:

```json
{"doc_id": "123", "title": "How to sort a list?", "tokens": ["how", "sort", "list"]}
```

### Шаг 2: Индексация (C)

```bash
./app index --type=avl
./app index --type=rb
./app index --type=btree
```

Программа читает `data/processed/docs.jsonl`, вставляет каждый токен в выбранное дерево и сохраняет индекс в `data/index_<type>.txt`.

```c
// index.h
typedef enum { TREE_AVL, TREE_RB, TREE_BTREE } TreeType;

typedef struct {
    void*    tree;
    TreeType type;
} Index;

Index*  createIndex(TreeType type);
void    indexDocument(Index* idx, int doc_id, const char* title,
                      const char** tokens, int n_tokens);
Vector* lookupTerm(const Index* idx, const char* term);
void    saveIndex(const Index* idx, const char* path);
Index*  loadIndex(const char* path, TreeType type);
void    freeIndex(Index* idx);
```

### Шаг 3: Поиск

```bash
./app search --type=avl "balanced tree rotation"
./app search --type=rb "python list sort"
./app search --type=btree "memory leak segfault"
```

**Логика:**

1. Разбить запрос на токены
2. Для каждого токена получить posting list из дерева
3. Пересечь списки (AND-семантика)
4. Вывести топ-10

```c
// search.h
typedef struct {
    int  doc_id;
    char title[256];
    int  score;
} SearchResult;

typedef struct {
    Vector* results;   /* elements: SearchResult, топ-10 */
    int     total;     /* всего найдено документов       */
    double  time_ms;
} SearchResults;

Vector*        intersectPostings(Vector** lists, int n);
SearchResults* search(Index* idx, const char* query);
void           printResultsText(const SearchResults* sr);
void           printResultsJSON(const SearchResults* sr);
void           freeSearchResults(SearchResults* sr);
```

Итерация по результатам:

```c
for (size_t i = 0; i < sr->results->size; i++) {
    SearchResult* r = getVectorItem(sr->results, i);
    printf("[id=%d] %s\n", r->doc_id, r->title);
}
```

**Пример вывода:**

```
Время: 2.1 мс | Найдено: 47 документов

 1. [id=1234] How to sort a list of objects in Python?
 2. [id=5678] Python sort() vs sorted(): what's the difference?
 3. [id=9012] Sort list of dictionaries by value in Python
 ...
```

---

## Задача 3: Эксперименты

Сравнить три структуры на трёх метриках.

### 3.1 Время индексации

Индексировать 50k / 200k / 500k документов, замерить время.

| Документов | AVL (сек) | Red-Black (сек) | B-tree (сек) |
|---|---|---|---|
| 50k | ? | ? | ? |
| 200k | ? | ? | ? |
| 500k | ? | ? | ? |

### 3.2 Время поиска

Выполнить 1000 запросов разной длины (1, 2, 3 слова).

| Слов в запросе | AVL (мс/запрос) | Red-Black (мс/запрос) | B-tree (мс/запрос) |
|---|---|---|---|
| 1 | ? | ? | ? |
| 2 | ? | ? | ? |
| 3 | ? | ? | ? |

### 3.3 Использование памяти

```c
#include <sys/resource.h>

struct rusage usage;
getrusage(RUSAGE_SELF, &usage);
printf("Memory: %ld KB\n", usage.ru_maxrss);
```

| Документов | AVL (МБ) | Red-Black (МБ) | B-tree (МБ) |
|---|---|---|---|
| 50k | ? | ? | ? |
| 500k | ? | ? | ? |

---

## Задача 4 (hard): Fuzzy Search

Реализовать нечёткий поиск через расстояние Левенштейна.

**Алгоритм:**

1. Для каждого слова запроса обойти всё дерево и собрать кандидатов с расстоянием ≤ max_dist
2. Для каждого кандидата получить posting list
3. Объединить результаты, ранжировать по `(score, distance)`

```c
typedef struct {
    char    term[256];
    int     distance;
    Vector* postings;
} FuzzyCandidate;

/* Список кандидатов — Vector с элементами FuzzyCandidate */
Vector* fuzzyFindCandidates(Index* idx, const char* term, int max_distance);

/* Нечёткий поиск с ранжированием */
SearchResults* fuzzySearch(Index* idx, const char* query, int max_distance);
```

Работа со списком кандидатов:

```c
Vector* cl = fuzzyFindCandidates(idx, "pythn", 2);
for (size_t i = 0; i < cl->size; i++) {
    FuzzyCandidate* c = getVectorItem(cl, i);
    printf("term=%s dist=%d\n", c->term, c->distance);
}
vectorFree(cl);
```

**Ранжирование:** `score = matched_terms * 10 - avg_distance`

---

## Структура решения

```
lab14/
├── README.md
├── data/                   # датасет (добавить в .gitignore)
│   ├── Questions.csv
│   ├── Answers.csv
│   └── processed/
│       └── docs.jsonl
│
├── avl/
│   ├── avl.h
│   ├── avl.c
│   └── tests.c
│
├── rbtree/
│   ├── rbtree.h
│   ├── rbtree.c
│   └── tests.c
│
├── btree/
│   ├── btree.h
│   ├── btree.c
│   └── tests.c
│
├── index/
│   ├── index.h
│   ├── index.c
│   ├── search.h
│   └── search.c
│
├── preprocess.py
├── app.py                  # Streamlit-интерфейс
├── main.c
└── Makefile
```

---

## Сборка и запуск

### Сборка

```bash
# Всё сразу
make

# Только приложение
make app

# Тесты
make test_avl && ./test_avl
make test_rb && ./test_rb
make test_btree && ./test_btree
```

### Препроцессинг

```bash
python preprocess.py --input data/Questions.csv --output data/processed/docs.jsonl
```

### Индексация

```bash
./app index --type=avl
./app index --type=rb
./app index --type=btree
```

### Поиск

```bash
./app search --type=avl "memory leak"
./app search --type=rb "python list sort"
./app search --type=btree "balanced binary tree"
```

### Streamlit-интерфейс

Через docker-compose (рекомендуется):

```bash
# 1. Один раз: сбилдить образ
docker-compose build

# 2. Предварительно проиндексировать данные (индекс пишется в data/ — она примонтирована)
docker-compose run --rm search \
    python3 preprocess.py --input data/Questions.csv --output data/processed/docs.jsonl

docker-compose run --rm search ./app index --type=avl
docker-compose run --rm search ./app index --type=rb
docker-compose run --rm search ./app index --type=btree

# 3. Запустить веб-интерфейс
docker-compose up
```

Открыть в браузере: http://localhost:8501

Без Docker:

```bash
uv run app.py
```

---

## Полезные ресурсы

1. [AVL Trees — Visualgo](https://visualgo.net/en/bst)
2. [Red-Black Tree — CLRS, глава 13](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
3. [B-Trees — Wikipedia](https://en.wikipedia.org/wiki/B-tree)
4. [Inverted Index — Wikipedia](https://en.wikipedia.org/wiki/Inverted_index)

---

**P.S.** Все три дерева логарифмические, но дьявол — в константах. AVL ротирует педантично, Red-Black чуть расслаблен, B-tree забирает целую страницу памяти и чувствует себя прекрасно. Настоящий поисковый движок — это Red-Black для индекса в памяти и B-tree, когда данные не влезают в RAM. Реализуете оба — поймёте почему.
