#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256
#define TLB_SIZE 16
#define OFFSET_MASK 255

struct page_entry {
    int valid;
    int frame_number;
};

struct tlb_entry {
    int page_number;
    int frame_number;
};

FILE *backing_store;
char memory[NUM_PAGES][PAGE_SIZE];
struct page_entry page_table[NUM_PAGES];
struct tlb_entry tlb[TLB_SIZE];

int tlb_index = 0;// для записи в TBL
int frame_index = 0;//указывает на индекс фрейма в массиве оперативной памяти memory, где хранится данные
int tlb_hits = 0;//учет попаданий в TBL
int page_faults = 0;//учет не попаданий в TBL

int load_page(int page_number) {
    if (frame_index >= NUM_PAGES) {
        fprintf(stderr, "No more frames available\n");//если достигли максимум фреймоы
        return -1;
    }
    fseek(backing_store, page_number * PAGE_SIZE, SEEK_SET);//устанавливаем указатель на начало нужной страницы
    fread(memory[frame_index], sizeof(char), PAGE_SIZE, backing_store);//считываем

    page_table[page_number].valid = 1;//теперь есть в таблице
    page_table[page_number].frame_number = frame_index;

    return frame_index++;
}

void update_tlb(int page_number, int frame_number) {
    tlb[tlb_index].page_number = page_number;
    tlb[tlb_index].frame_number = frame_number;
    tlb_index = (tlb_index + 1) % TLB_SIZE;// добавляем всегда последним , если привысим 16 , начнем с начала. LRU
}

int find_in_tlb(int page_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].page_number == page_number) {
            tlb_hits++;
            return tlb[i].frame_number;
        }
    }
    return -1;
}

char get_byte(int logical_address) {
    int page_number = logical_address >> 8;
    int offset = logical_address & OFFSET_MASK;

    int frame_number = find_in_tlb(page_number);//находим в tbl

    if (frame_number == -1) {
        if (!page_table[page_number].valid) {// если нет в тбл , ищем в таблице
            frame_number = load_page(page_number);//если нет , то загружаем из backing store
            page_faults++;
        } else {
            frame_number = page_table[page_number].frame_number;//если есть , загружаем
        }
        update_tlb(page_number, frame_number);//функция для добавления записи в tbl
    }

    return memory[frame_number][offset];
}

int main(int argc, char *argv[]) {

    if (argc != 2) {// не ввели аргумент
        fprintf(stderr, "Error. Enter the file.");
        return -1;
    }

    backing_store = fopen("BACKING_STORE.bin", "rb");// ошибки открытия файлов
    if (!backing_store) {
        fprintf(stderr, "Error opening BACKING_STORE.bin\n");
        return -1;
    }

    memset(page_table, 0, sizeof(page_table));
    memset(tlb, -1, sizeof(tlb));//инициализация таблицы и tlb

    FILE *address_file = fopen(argv[1], "r");
    if (!address_file) {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }
    FILE *output_file = fopen("prog_out.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error opening file out file\n");
        return -1;
    }

    int logical_address, processed = 0;// для статистики и переменная
    while (fscanf(address_file, "%d", &logical_address) == 1) {
        char value = get_byte(logical_address);//функция для вывода значения
        printf("Virtual address: %d Physical address: %d Value: %d\n",
               logical_address, (page_table[logical_address >> 8].frame_number << 8) | (logical_address & OFFSET_MASK), value);//физический адрес из таблицы
        fprintf(output_file,"Virtual address: %d Physical address: %d Value: %d\n",
               logical_address, (page_table[logical_address >> 8].frame_number << 8) | (logical_address & OFFSET_MASK), value);//физический адрес из таблицы
        processed++;
    }

    printf("\nProcessed addresses = %d\n", processed);
    printf("Page fault frequency = %.2f%%\n", (float)page_faults / processed * 100);
    printf("TLB hit frequency = %.2f%%\n", (float)tlb_hits / processed * 100);

    fclose(address_file);
    fclose(backing_store);
    fclose(output_file);
    return 0;
}
