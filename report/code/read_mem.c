int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		pthread_mutex_lock(&ram_lock); // add
		*data = _ram[physical_addr]; 
		pthread_mutex_unlock(&ram_lock); // add
		return 0;
	}else{
		return 1;
	}
}
