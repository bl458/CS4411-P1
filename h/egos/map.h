struct map *map_init(void);
void **map_insert(struct map **pmap, const void *key, unsigned int key_size);
void *map_lookup(struct map *map, const void *key, unsigned int key_size);
void map_iter(void *env, struct map *map, void (*upcall)(void *env,
						const void *key, unsigned int key_size, void *value));
void map_release(struct map *map);
void map_cleanup(void);

unsigned int sdbm_hash(const void *key, unsigned int key_size);
