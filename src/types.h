#pragma once
#ifndef SU_TYPES_H
#define SU_TYPES_H

#include "kigu/array.h"
#include "kigu/common.h"
#include "kigu/string.h"
#include "core/threading.h"
#include "ctype.h"

#define SetThreadName(...) DeshThreadManager->set_thread_name(suStr8(__VA_ARGS__))

//attempt at making str8 building thread safe
#define suStr8(...) to_str8_su(__VA_ARGS__)
template<class...T>
str8 to_str8_su(T... args){DPZoneScoped;
	persist mutex tostr8_lock = init_mutex();
	tostr8_lock.lock();
	global_mem_lock.lock();
	str8b str; str8_builder_init(&str, {0}, deshi_temp_allocator);
	constexpr auto arg_count{sizeof...(T)};
	str8 arr[arg_count] = {to_str8(args, deshi_temp_allocator)...};
	forI(arg_count) str8_builder_append(&str, arr[i]);
	global_mem_lock.unlock();
	tostr8_lock.unlock();
	return str.fin;
}


//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Compile Options
enum ReturnCode {
	ReturnCode_Success                = 0,
	ReturnCode_No_File_Passed         = 1,
	ReturnCode_File_Not_Found         = 2,
	ReturnCode_File_Locked            = 3,
	ReturnCode_File_Invalid_Extension = 4,
	ReturnCode_Invalid_Argument       = 5,
	ReturnCode_Lexer_Failed           = 6,
	ReturnCode_Preprocessor_Failed    = 7,
	ReturnCode_Parser_Failed          = 8,
	ReturnCode_Assembler_Failed       = 9,
};

enum{
	W_NULL = 0,
	//// @level1 //// (warnings that are probably programmer errors, but are valid in rare cases)
	W_Level1_Start = W_NULL,

	WC_Overflow,
	//WC_Implicit_Narrowing_Conversion,

	W_Level1_End,
	//// @level2 //// (all the other warnings)
	W_Level2_Start = W_Level1_End,



	W_Level2_End,
	//// @level3 //// (warnings you might want to be aware of, but are valid in most cases)
	W_Level3_Start = W_Level2_End,
	
	WC_Empty_Import_Directive,

	WC_Unreachable_Code_After_Return,
	WC_Unreachable_Code_After_Break,
	WC_Unreachable_Code_After_Continue,
	//WC_Negative_Constant_Assigned_To_Unsigned_Variable,

	W_Level3_End,
	W_COUNT = W_Level3_End
};

enum OSOut {
	OSOut_Windows,
	OSOut_Linux,
	OSOut_OSX,
};

/*	Verbosity
	0: Does not display any information other than warnings, errors, and notes that may come with them
	1: Shows each files name as it is reached in compilation 
	2: Shows the stages as they happen and the time it took for them to complete
	3: Shows parts of the stages as they happen
	4: Shows even more detail about whats happening in each stage
	5: Shows compiler debug information from each stage
*/

enum{
	Verbosity_Always,
	Verbosity_FileNames,
	Verbosity_Stages,
	Verbosity_StageParts,
	Verbosity_Detailed,
	Verbosity_Debug,
};

struct {
	u32 warning_level = 1;
	u32 verbosity = Verbosity_Debug;
	u32 indent = 0;
	b32 supress_warnings   = false;
	b32 supress_messages   = false;
	b32 warnings_as_errors = false;
	b32 show_code = true;
	b32 log_immediatly           = true;
	b32 assert_compiler_on_error = true;
	OSOut osout = OSOut_Windows;
} globals;

enum{
	Message_Log,
	Message_Error,
	Message_Warn,
	Message_Note
};

mutex global_mem_lock = init_mutex();

void* su_memalloc(upt size){
	global_mem_lock.lock();
	void* ret = memalloc(size);
	global_mem_lock.unlock();
	return ret;
}

void su_memzfree(void* ptr){
	global_mem_lock.lock();
	memzfree(ptr);
	global_mem_lock.unlock();
}

//attempt at implementing a thread safe memory region 
//this is chunked, so memory never moves unless you use something like remove
//TODO(sushi) non-chunked variant
template<typename T>
struct suChunkedArena{
	Arena** arenas;
	u64 arena_count = 0;
	u64 arena_space = 16;
	u64 count = 0;
	mutex write_lock;
	mutex read_lock;

	void init(upt n_obj_per_chunk){
		write_lock.init();
		read_lock.init();
		arena_count = 1;
		arenas = (Arena**)su_memalloc(sizeof(Arena*)*arena_space);
		global_mem_lock.lock();
		arenas[arena_count-1] = memory_create_arena(sizeof(T)*n_obj_per_chunk); 
		global_mem_lock.unlock();
	}

	void deinit(){
		global_mem_lock.lock();
		forI(arena_count){
			memory_delete_arena(arenas[i]);
		}
		global_mem_lock.unlock();
		su_memzfree(arenas);
	}

	T* add(const T& in){
		write_lock.lock();

		//make a new arena if needed
		if(arenas[arena_count-1]->used + sizeof(T) > arenas[arena_count-1]->size){
			global_mem_lock.lock();
			if(arena_space == arena_count){
				arena_space += 16;
				arenas = (Arena**)memrealloc(arenas, arena_space*sizeof(Arena*));
			}
			arenas[arena_count] = memory_create_arena(arenas[arena_count-1]->size);
			arena_count++;
			global_mem_lock.unlock();
		}
		count++;
		memcpy(arenas[arena_count-1]->cursor, &in, sizeof(T));
		T* ret = (T*)arenas[arena_count-1]->cursor;
		arenas[arena_count-1]->used += sizeof(T);
		arenas[arena_count-1]->cursor += sizeof(T);
		write_lock.unlock();
		return ret;
	}

	T read(upt idx){
		persist u64 read_count = 0;
		read_lock.lock();
		read_count++;
		if(read_count==1){
			write_lock.lock();
		}
		read_lock.unlock();

		Assert(idx < count);

		u64 chunk_size = arenas[0]->size;
		u64 offset = idx * sizeof(T);
		u64 arenaidx = offset / chunk_size;

		T ret = *(T*)(arenas[arenaidx]->start + (offset - arenaidx * chunk_size));

		read_lock.lock();
		read_count--;
		if(!read_count){
			write_lock.unlock();
		}
		read_lock.unlock();

		return ret; 
	}

	void remove(upt idx){
		write_lock.lock();
		Assert(idx < count);

		u64 chunk_size = arenas[0]->size;
		u64 offset = idx * sizeof(T);
		u64 arenaidx = offset / chunk_size;
		T* removee = (T*)(arenas[arenaidx]->start + (offset-arenaidx*chunk_size));

		memmove(removee, removee+1, chunk_size - (offset - arenaidx * chunk_size));

		forI(arena_count - arenaidx){
			u64 idx = arenaidx + i;
			if(idx+1 < arena_count){
				//there is another arena ahead of this one, so we replace this one's last value with its first
				//and then move it back one
				memcpy(arenas[idx]->start + arenas[idx]->size-sizeof(T), arenas[idx+1]->start, sizeof(T));
				memmove(arenas[idx+1]->start, arenas[idx+1]->start+sizeof(T), arenas[idx+1]->used-sizeof(T));
				arenas[idx+1]->used -= sizeof(T);
			}else{
				//this is the last arena so just move its data back
				memmove(arenas[idx]->start, arenas[idx]->start+sizeof(T), arenas[idx]->used-sizeof(T));
				arenas[idx]->used -= sizeof(T);
				arenas[idx]->cursor -= sizeof(T);
			}
		}

		write_lock.unlock();
	}

};

template<typename T>
struct suArena{
	T* data;
	mutex write_lock;
	mutex read_lock;
	u64 count = 0;
	u64 space = 0;

	void init(upt initial_size = 16){DPZoneScoped;
		write_lock.init();
		read_lock.init();
		global_mem_lock.lock();
		space = initial_size;
		data = (T*)memalloc(sizeof(T)*space); 
		global_mem_lock.unlock();
	}

	void deinit(){DPZoneScoped;
		write_lock.deinit();
		read_lock.deinit();
		global_mem_lock.lock();
		memzfree(data);
		global_mem_lock.unlock();
		count = 0;
		space = 0;
	}

	void add(const T& in){DPZoneScoped;
		write_lock.lock();
		defer{write_lock.unlock();};
		if(count == space){
			global_mem_lock.lock();
			space += 16;
			data = (T*)memrealloc(data, sizeof(T)*space);
			global_mem_lock.unlock();
		}
		memcpy(data+count, &in, sizeof(T));
		count++;
	}

	//TODO(sushi) need to implement a system for allowing an arbitrary amount of threads to read while blocking writing
	T read(upt idx) {DPZoneScoped;
		write_lock.lock();
		defer{write_lock.unlock();};

		Assert(idx < count);

		T ret = *(data + idx);

		return ret; 
	}

	void modify(upt idx, const T& val){DPZoneScoped;
		write_lock.lock();
		defer{write_lock.unlock();};
		Assert(idx < count)
		memcpy(data+idx, &val, sizeof(T));
	}

	void insert(upt idx, const T& val){DPZoneScoped;
		write_lock.lock();
		defer{write_lock.unlock();};
		Assert(idx <= count);
		if(idx == count){
			add(val);
		} else if(count == space){
			global_mem_lock.lock();
			space += 16;
			data = (T*)memrealloc(data, sizeof(T)*space);
			global_mem_lock.unlock();
			memmove(data+idx+1, data+idx, (count-idx)*sizeof(T));
			memcpy(data+idx, &val, sizeof(T));
			count++;
		}else{
			memmove(data+idx+1, data+idx, (count-idx)*sizeof(T));
			memcpy(data+idx, &val, sizeof(T));
			count++;
		}
	}

	T operator [](u64 idx){DPZoneScoped;
		return read(idx);
	}

	T* readptr(u64 idx){
		write_lock.lock();
		defer{write_lock.unlock();};
		//NOTE(sushi) its probably possible that once the thread gets into here 
		//            that another thread has removed enough elements to make this idx invalid
		//            the solution is to just avoid situations where this would happen
		Assert(idx < count);

		T* ret = data + idx;

		return ret;
	}

	void remove(upt idx){DPZoneScoped;
		write_lock.lock();
		defer{write_lock.unlock();};
		Assert(idx < count);
		count--;
		memmove(data + idx, data + idx + 1, count - idx);
	}

	void remove_unordered(upt idx){
		write_lock.lock();
		defer{write_lock.unlock();};
		Assert(idx < count);
		T endval = data[count-1];
		count--;
		if(count){
			data[idx] = endval;
		}
	}

	T pop(u64 _count = 1){ 
		write_lock.lock();
		defer{write_lock.unlock();};
		Assert(_count <= count);
		T ret;
		forI(_count){
			if(i==_count-1) memcpy(&ret, data+count-1, sizeof(T));
			count--;
		}
		return ret;
	}

	inline T* begin(){ return &data[0]; }
	inline T* end()  { return &data[count]; }
	inline const T* begin()const{ return &data[0]; }
	inline const T* end()  const{ return &data[count]; }
};


//sorted binary searched map for matching identifiers with
//their declarations. this map supports storing keys who have collided
//by storing them as neighbors and storing the unhashed key with the value
//this was delle's idea
//TODO(sushi) decide if we should store pair<str8,Decl*> in data or just use the str8 `identifier`
//            that is on Declaration already, this way we just pass a Declaration* and no str8
struct Declaration;
struct declmap{
	suArena<u64> hashes; 
	suArena<pair<str8, Declaration*>> data;

	void init(){DPZoneScoped;
		hashes.init(256);
		data.init(256);
	}

	u32 find_key(u64 key){DPZoneScoped;
		spt index = -1;
		spt middle = -1;
		if(hashes.count){
			spt left = 0;
			spt right = hashes.count-1;
			while(left <= right){
				middle = left+((right-left)/2);
				if(hashes[middle] == key){
					index = middle;
					break;
				}
				if(hashes[middle] < key){
					left = middle+1;
					middle = left+((right-left)/2);
				}else{
					right = middle-1;
				}
			}
		}
		return index;
	}

	u32 find_key(str8 key){DPZoneScoped;
		return find_key(str8_hash64(key));
	}

	//note there is no overload for giving the key as a u64 because this struct requires
	//storing the original key value in the data.
	u32 add(str8 key, Declaration* val){DPZoneScoped;
		u64 key_hash = str8_hash64(key);
		spt index = -1;
		spt middle = 0;
		if(hashes.count){
			spt left = 0;
			spt right = hashes.count-1;
			while(left <= right){
				middle = left+((right-left)/2);
				if(hashes[middle] == key_hash){
					index = middle;
					break;
				}
				if(hashes[middle] < key_hash){
					left = middle+1;
					middle = left+((right-left)/2);
				}else{
					right = middle-1;
				}
			}
		}
		//if the index was found AND this is not a collision, we can just return 
		//but if the second check fails then this is a collision and we must still insert it as a neighbor 
		if(index != -1 && str8_equal_lazy(key, data[index].first)){
			return index;
		}else{
			hashes.insert(middle, key_hash);
			data.insert(middle, {key, val});
			return middle;
		}
	}

	b32 has(str8 key){DPZoneScoped;
		return (find_key(key) == -1 ? 0 : 1);
	}

	b32 has_collided(u64 key){DPZoneScoped;
		u32 index = find_key(key);
		if(index != -1 && 
		   index > 0            && hashes[index-1] != hashes[index] &&
	       index < hashes.count && hashes[index+1] != hashes[index]){
			return true;
		}
		return false;
	}

	//find the first instance of a key in the hashes array to simplify looking for a match
	//when things have collided. I'm not sure this is the best way to go about this.
	//this expects the duplicated key to have already been found 
	u32 find_key_first_entry(u64 idx){DPZoneScoped;
		while(idx!=0 && hashes[idx-1] == hashes[idx]) idx--;
		return idx;
	}

	//tests if the hash array at the found index has neighbors that are the same
	b32 has_collided(str8 key){DPZoneScoped;
		return has_collided(str8_hash64(key));
	}

	Declaration* at(str8 key){DPZoneScoped;
		u32 index = find_key(key);
		if(index != -1){
			if(hashes.count == 1){
				return data[0].second;
			}else if(index && index < hashes.count-1 && hashes[index-1] != hashes[index] && hashes[index+1] != hashes[index]){
				return data[index].second;
			}else if (!index && hashes.count > 1 && hashes[1] != hashes[0]){
				return data[index].second;
			}else if(index == hashes.count-1 && hashes[index-1] != hashes[index]){
				return data[index].second;
			}else{
				Log("declmap", "A collision happened in a declmap, which is NOT tested, so you should test this if it appears");
				TestMe;
				//a collision happened so we must find the right neighbor
				u32 idx = index;
				//iterate backwards as far as we can
				while(idx!=0 && hashes[idx-1] == hashes[idx]){
					idx--;
					if(str8_equal_lazy(data[idx].first, key)){
						return data[idx].second;
					}
				}
				idx = index;
				//iterate forwards as far as we can
				while(idx!=hashes.count-1 && hashes[idx+1] == hashes[idx]){
					idx++;
					if(str8_equal_lazy(data[idx].first, key)){
						return data[idx].second;
					}
				}
				return 0;
			}
		}
		if(index != -1){
			return data[index].second;
		}else{
			return 0;
		}
		return 0;
	}

	Declaration* atIdx(u64 idx){
		Assert(idx < data.count);
		return data[idx].second;
	}
};


//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Nodes
enum NodeType : u32 {
	NodeType_Program,
	NodeType_Structure,
	NodeType_Function,
	NodeType_Variable,
	NodeType_Scope,
	NodeType_Statement,
	NodeType_Expression,
};

//thread safe version of TNode (probably)
//this only accounts for modifying a node, not reading it
//but I believe that modification and reading shouldnt happen at the same time, so its probably fine
//TODO(sushi) decide if this is necessary, the only place threading becomes a problem with this is parser adding stuff to its base node
//            but that can be deferred.
struct suNode{
	Type  type;
	Flags flags;
	
	suNode* next = 0;
	suNode* prev = 0;
	suNode* parent = 0;
	suNode* first_child = 0;
	suNode* last_child = 0;
	u32     child_count = 0;
	
	mutex lock;

	str8 debug;
};

global inline void insert_after(suNode* target, suNode* node) { DPZoneScoped;
	target->lock.lock();
	node->lock.lock();
	defer{
		node->lock.unlock();
		target->lock.unlock();
	};
	if (target->next) target->next->prev = node;
	node->next = target->next;
	node->prev = target;
	target->next = node;
	
}

global inline void insert_before(suNode* target, suNode* node) { DPZoneScoped;
	target->lock.lock();
	node->lock.lock();
	defer{
		target->lock.unlock();
		node->lock.unlock();
	};
	if (target->prev) target->prev->next = node;
	node->prev = target->prev;
	node->next = target;
	target->prev = node;
}

global inline void remove_horizontally(suNode* node) { DPZoneScoped;
	node->lock.lock();
	defer {node->lock.unlock();};
	if (node->next) node->next->prev = node->prev;
	if (node->prev) node->prev->next = node->next;
	node->next = node->prev = 0;
}

global void insert_last(suNode* parent, suNode* child) { DPZoneScoped;
	parent->lock.lock();
	child->lock.lock();
	defer {
		parent->lock.unlock();
		child->lock.unlock();
	};

	if (parent == 0) { child->parent = 0; return; }
	if(parent==child){DebugBreakpoint;}
	
	child->parent = parent;
	if (parent->first_child) {
		insert_after(parent->last_child, child);
		parent->last_child = child;
	}
	else {
		parent->first_child = child;
		parent->last_child = child;
	}
	parent->child_count++;
}

global void insert_first(suNode* parent, suNode* child) { DPZoneScoped;
	parent->lock.lock();
	child->lock.lock();
	defer{
		parent->lock.unlock();
		child->lock.unlock();
	};
	if (parent == 0) { child->parent = 0; return; }
	
	child->parent = parent;
	if (parent->first_child) {
		insert_before(parent->first_child, child);
		parent->first_child = child;
	}
	else {
		parent->first_child = child;
		parent->last_child = child;
	}
	parent->child_count++;
	
}

global void change_parent(suNode* new_parent, suNode* node) { DPZoneScoped;
	new_parent->lock.lock();
	node->lock.lock();
	defer {
		new_parent->lock.unlock();
		node->lock.unlock();
	};
	//if old parent, remove self from it 
	if (node->parent) {
		if (node->parent->child_count > 1) {
			if (node == node->parent->first_child) node->parent->first_child = node->next;
			if (node == node->parent->last_child)  node->parent->last_child = node->prev;
		}
		else {
			Assert(node == node->parent->first_child && node == node->parent->last_child, "if node is the only child node, it should be both the first and last child nodes");
			node->parent->first_child = 0;
			node->parent->last_child = 0;
		}
		node->parent->child_count--;
	}
	
	//remove self horizontally
	remove_horizontally(node);
	
	//add self to new parent
	insert_last(new_parent, node);
}

global void move_to_parent_first(suNode* node){ DPZoneScoped;
	node->lock.lock();
	defer { node->lock.unlock(); };
	if(!node->parent) return;
	
	suNode* parent = node->parent;
	if(parent->first_child == node) return;
	if(parent->last_child == node) parent->last_child = node->prev;

	remove_horizontally(node);
	node->next = parent->first_child;
	parent->first_child->prev = node;
	parent->first_child = node;
}

global void move_to_parent_last(suNode* node){ DPZoneScoped;
	node->lock.lock();
	defer{node->lock.unlock();};
	if(!node->parent) return;
	
	suNode* parent = node->parent;
	if(parent->last_child == node) return;
	if(parent->first_child == node) parent->first_child = node->next;

	remove_horizontally(node);
	node->prev = parent->last_child;
	parent->last_child->next = node;
	parent->last_child = node;
}

global void remove(suNode* node) { DPZoneScoped;
	node->lock.lock();
	defer{node->lock.unlock();};
	//add children to parent (and remove self from children)
	for(suNode* it = node->first_child; it != 0; ) {
		suNode* next = it->next;
		change_parent(node->parent, it);
		it = next;
	}
	
	//remove self from parent
	if (node->parent) {
		if (node->parent->child_count > 1) {
			if (node == node->parent->first_child) node->parent->first_child = node->next;
			if (node == node->parent->last_child)  node->parent->last_child = node->prev;
		}
		else {
			Assert(node == node->parent->first_child && node == node->parent->last_child, "if node is the only child node, it should be both the first and last child nodes");
			node->parent->first_child = 0;
			node->parent->last_child = 0;
		}
		node->parent->child_count--;
	}
	node->parent = 0;
	
	//remove self horizontally
	remove_horizontally(node);
	node->lock.unlock();
}

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Registers
enum Registers{
	Register_NULL,
	
	Register0_64,  Register0_32,  Register0_16,  Register0_8,
	Register1_64,  Register1_32,  Register1_16,  Register1_8,
	Register2_64,  Register2_32,  Register2_16,  Register2_8,
	Register3_64,  Register3_32,  Register3_16,  Register3_8,
	Register4_64,  Register4_32,  Register4_16,  Register4_8,
	Register5_64,  Register5_32,  Register5_16,  Register5_8,
	Register6_64,  Register6_32,  Register6_16,  Register6_8,
	Register7_64,  Register7_32,  Register7_16,  Register7_8,
	Register8_64,  Register8_32,  Register8_16,  Register8_8,
	Register9_64,  Register9_32,  Register9_16,  Register9_8,
	Register10_64, Register10_32, Register10_16, Register10_8,
	Register11_64, Register11_32, Register11_16, Register11_8,
	Register12_64, Register12_32, Register12_16, Register12_8,
	Register13_64, Register13_32, Register13_16, Register13_8,
	Register14_64, Register14_32, Register14_16, Register14_8,
	Register15_64, Register15_32, Register15_16, Register15_8,
	
	//x64 names
	Register_RAX = Register0_64,  Register_EAX  = Register0_32,  Register_AX   = Register0_16,  Register_AL   = Register0_8,
	Register_RDX = Register1_64,  Register_EDX  = Register1_32,  Register_DX   = Register1_16,  Register_DL   = Register1_8,
	Register_RCX = Register2_64,  Register_ECX  = Register2_32,  Register_CX   = Register2_16,  Register_CL   = Register2_8,
	Register_RBX = Register3_64,  Register_EBX  = Register3_32,  Register_BX   = Register3_16,  Register_BL   = Register3_8,
	Register_RSI = Register4_64,  Register_ESI  = Register4_32,  Register_SI   = Register4_16,  Register_SIL  = Register4_8,
	Register_RDI = Register5_64,  Register_EDI  = Register5_32,  Register_DI   = Register5_16,  Register_DIL  = Register5_8,
	Register_RSP = Register6_64,  Register_ESP  = Register6_32,  Register_SP   = Register6_16,  Register_SPL  = Register6_8,
	Register_RBP = Register7_64,  Register_EBP  = Register7_32,  Register_BP   = Register7_16,  Register_BPL  = Register7_8,
	Register_R8  = Register8_64,  Register_R8D  = Register8_32,  Register_R8W  = Register8_16,  Register_R8B  = Register8_8,
	Register_R9  = Register9_64,  Register_R9D  = Register9_32,  Register_R9W  = Register9_16,  Register_R9B  = Register9_8,
	Register_R10 = Register10_64, Register_R10D = Register10_32, Register_R10W = Register10_16, Register_R10B = Register10_8,
	Register_R11 = Register11_64, Register_R11D = Register11_32, Register_R11W = Register11_16, Register_R11B = Register11_8,
	Register_R12 = Register12_64, Register_R12D = Register12_32, Register_R12W = Register12_16, Register_R12B = Register12_8,
	Register_R13 = Register13_64, Register_R13D = Register13_32, Register_R13W = Register13_16, Register_R13B = Register13_8,
	Register_R14 = Register14_64, Register_R14D = Register14_32, Register_R14W = Register14_16, Register_R14B = Register14_8,
	Register_R15 = Register15_64, Register_R15D = Register15_32, Register_R15W = Register15_16, Register_R15B = Register15_8,
	
	//usage
	Register_FunctionReturn     = Register_RAX,
	Register_BasePointer        = Register_RBP,
	Register_StackPointer       = Register_RSP,
	Register_FunctionParameter0 = Register_RDI,
	Register_FunctionParameter1 = Register_RSI,
	Register_FunctionParameter2 = Register_RDX,
	Register_FunctionParameter3 = Register_RCX,
	Register_FunctionParameter4 = Register_R8,
	Register_FunctionParameter5 = Register_R9,
};

global const char* registers_x64[] = {
	"%null",
	"%rax", "%eax",  "%ax",   "%al",
	"%rdx", "%edx",  "%dx",   "%dl",
	"%rcx", "%ecx",  "%cx",   "%cl",
	"%rbx", "%ebx",  "%bx",   "%bl",
	"%rsi", "%esi",  "%si",   "%sil",
	"%rdi", "%edi",  "%di",   "%dil",
	"%rsp", "%esp",  "%sp",   "%spl",
	"%rbp", "%ebp",  "%bp",   "%bpl",
	"%r8",  "%r8d",  "%r8w",  "%r8b",
	"%r9",  "%r9d",  "%r9w",  "%r9b",
	"%r10", "%r10d", "%r10w", "%r10b",
	"%r11", "%r11d", "%r11w", "%r11b",
	"%r12", "%r12d", "%r12w", "%r12b",
	"%r13", "%r13d", "%r13w", "%r13b",
	"%r14", "%r14d", "%r14w", "%r14b",
	"%r15", "%r15d", "%r15w", "%r15b",
};


//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Builtin Types
typedef u32 DataType; enum { 
	DataType_NotTyped,
	DataType_Void,       // void
	DataType_Implicit,   // implicitly typed
	DataType_Signed8,    // s8
	DataType_Signed16,   // s16
	DataType_Signed32,   // s32 
	DataType_Signed64,   // s64
	DataType_Unsigned8,  // u8
	DataType_Unsigned16, // u16
	DataType_Unsigned32, // u32 
	DataType_Unsigned64, // u64 
	DataType_Float32,    // f32 
	DataType_Float64,    // f64 
	DataType_String,     // str
	DataType_Any,
	DataType_Structure,  // data type of types and functions
}; 

const char* dataTypeStrs[] = {
	"notype",
	"void",
	"impl",  
	"s8",  
	"s16",
	"s32",  
	"s64",  
	"u8", 
	"u16",
	"u32",
	"u64",
	"f32",   
	"f64",   
	"str",    
	"ptr",   
	"any",
}; 

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Lexer
enum{
	Token_Null = 0,
	Token_ERROR = 0,                // when something doesnt make sense during lexing
	Token_EOF,                      // end of file
	
	TokenGroup_Identifier,
	Token_Identifier = TokenGroup_Identifier,  // function, variable and struct names                 
	
	//// literal ////
	TokenGroup_Literal,
	Token_LiteralFloat = TokenGroup_Literal,
	Token_LiteralInteger,
	Token_LiteralCharacter,
	Token_LiteralString,
	
	//// control ////
	TokenGroup_Control,
	Token_Semicolon = TokenGroup_Control, // ;
	Token_OpenBrace,                      // {
	Token_CloseBrace,                     // }
	Token_OpenParen,                      // (
	Token_CloseParen,                     // )
	Token_OpenSquare,                     // [
	Token_CloseSquare,                    // ]
	Token_Comma,                          // ,
	Token_QuestionMark,                   // ?
	Token_Colon,                          // :
	Token_Dot,                            // .
	Token_At,                             // @
	Token_Pound,                          // #
	Token_Backtick,                       // `
	
	//// operators ////
	TokenGroup_Operator,
	Token_Plus = TokenGroup_Operator, // +
	Token_Increment,                  // ++
	Token_PlusAssignment,             // +=
	Token_Negation,                   // -
	Token_Decrement,                  // --
	Token_NegationAssignment,         // -=
	Token_Multiplication,             // *
	Token_MultiplicationAssignment,   // *=
	Token_Division,                   // /
	Token_DivisionAssignment,         // /=
	Token_BitNOT,                     // ~
	Token_BitNOTAssignment,           // ~=
	Token_BitAND,                     // &
	Token_BitANDAssignment,           // &=
	Token_AND,                        // &&
	Token_BitOR,                      // |
	Token_BitORAssignment,            // |=
	Token_OR,                         // ||
	Token_BitXOR,                     // ^
	Token_BitXORAssignment,           // ^=
	Token_BitShiftLeft,               // <<
	Token_BitShiftLeftAssignment,     // <<=
	Token_BitShiftRight,              // >>
	Token_BitShiftRightAssignment,    // >>=
	Token_Modulo,                     // %
	Token_ModuloAssignment,           // %=
	Token_Assignment,                 // =
	Token_Equal,                      // ==
	Token_LogicalNOT,                 // !
	Token_NotEqual,                   // !=
	Token_LessThan,                   // <
	Token_LessThanOrEqual,            // <=
	Token_GreaterThan,                // >
	Token_GreaterThanOrEqual,         // >=
	
	//// keywords ////
	TokenGroup_Keyword,
	Token_Return = TokenGroup_Keyword, // return
	Token_If,                          // if
	Token_Else,                        // else
	Token_For,                         // for
	Token_While,                       // while 
	Token_Break,                       // break
	Token_Continue,                    // continue
	Token_Defer,                       // defer
	Token_StructDecl,                  // struct
	Token_This,                        // this
	Token_Using,                       // using
	Token_As,                          // as
	
	//// types  ////
	TokenGroup_Type,
	Token_Void = TokenGroup_Type, // void
	//NOTE(sushi) the order of these entries matter, primarily for type conversion reasons. see Validator::init
	Token_Unsigned8,              // u8
	Token_Unsigned16,             // u16
	Token_Unsigned32,             // u32 
	Token_Unsigned64,             // u64 
	Token_Signed8,                // s8
	Token_Signed16,               // s16 
	Token_Signed32,               // s32 
	Token_Signed64,               // s64
	Token_Float32,                // f32 
	Token_Float64,                // f64 
	Token_String,                 // str
	Token_Any,                    // any
	Token_Struct,                 // user defined type

	//// directives ////
	TokenGroup_Directive,
	Token_Directive_Import = TokenGroup_Directive,
	Token_Directive_Include,
	Token_Directive_Internal,
	Token_Directive_Run,

}; //typedef u32 Token_Type;

#define NAME(code) STRINGIZE(code)
const char* TokenTypes_Names[] = {
	NAME(Token_Null),
	NAME(Token_ERROR),
	NAME(Token_EOF),                      
	
	NAME(TokenGroup_Identifier),
	NAME(Token_Identifier),
	
	NAME(TokenGroup_Literal),
	NAME(Token_LiteralFloat),
	NAME(Token_LiteralInteger),
	NAME(Token_LiteralCharacter),
	NAME(Token_LiteralString),
	
	NAME(TokenGroup_Control),
	NAME(Token_Semicolon),
	NAME(Token_OpenBrace),                      
	NAME(Token_CloseBrace),                     
	NAME(Token_OpenParen),                      
	NAME(Token_CloseParen),                     
	NAME(Token_OpenSquare),                     
	NAME(Token_CloseSquare),                    
	NAME(Token_Comma),                          
	NAME(Token_QuestionMark),                   
	NAME(Token_Colon),                          
	NAME(Token_Dot),                            
	NAME(Token_At),                             
	NAME(Token_Pound),                          
	NAME(Token_Backtick),                       
	
	NAME(TokenGroup_Operator),
	NAME(Token_Plus),
	NAME(Token_Increment),                  
	NAME(Token_PlusAssignment),             
	NAME(Token_Negation),                   
	NAME(Token_Decrement),                  
	NAME(Token_NegationAssignment),         
	NAME(Token_Multiplication),             
	NAME(Token_MultiplicationAssignment),   
	NAME(Token_Division),                   
	NAME(Token_DivisionAssignment),         
	NAME(Token_BitNOT),                     
	NAME(Token_BitNOTAssignment),           
	NAME(Token_BitAND),                     
	NAME(Token_BitANDAssignment),           
	NAME(Token_AND),                        
	NAME(Token_BitOR),                      
	NAME(Token_BitORAssignment),            
	NAME(Token_OR),                         
	NAME(Token_BitXOR),                     
	NAME(Token_BitXORAssignment),           
	NAME(Token_BitShiftLeft),               
	NAME(Token_BitShiftLeftAssignment),     
	NAME(Token_BitShiftRight),              
	NAME(Token_BitShiftRightAssignment),    
	NAME(Token_Modulo),                     
	NAME(Token_ModuloAssignment),           
	NAME(Token_Assignment),                 
	NAME(Token_Equal),                      
	NAME(Token_LogicalNOT),                 
	NAME(Token_NotEqual),                   
	NAME(Token_LessThan),                   
	NAME(Token_LessThanOrEqual),            
	NAME(Token_GreaterThan),                
	NAME(Token_GreaterThanOrEqual),         
	
	NAME(TokenGroup_Keyword),
	NAME(Token_Return),
	NAME(Token_If),                          
	NAME(Token_Else),                        
	NAME(Token_For),                         
	NAME(Token_While),                       
	NAME(Token_Break),                       
	NAME(Token_Continue),                    
	NAME(Token_Defer),                       
	NAME(Token_StructDecl),                  
	NAME(Token_This),                        
	NAME(Token_Using),                       
	NAME(Token_As),                          
	
	NAME(TokenGroup_Type),
	NAME(Token_Void),
	NAME(Token_Signed8),                
	NAME(Token_Signed16),               
	NAME(Token_Signed32),               
	NAME(Token_Signed64),               
	NAME(Token_Unsigned8),              
	NAME(Token_Unsigned16),             
	NAME(Token_Unsigned32),             
	NAME(Token_Unsigned64),             
	NAME(Token_Float32),                
	NAME(Token_Float64),                
	NAME(Token_String),                 
	NAME(Token_Any),                    
	NAME(Token_Struct),                 
	
	NAME(TokenGroup_Directive),
	NAME(Token_Directive_Import),
	NAME(Token_Directive_Include),
	NAME(Token_Directive_Internal),
	NAME(Token_Directive_Run),
};
#undef NAME

struct Token {
	Type type;
	Type group;
	str8 raw; 
	u64  raw_hash;
	
	str8 file;
	u32 l0, l1;
	u32 c0, c1;
	u8* line_start;

	u32 scope_depth;
	u32 idx;

	b32 is_global; //set true on tokens that are in global scope 
	b32 is_declaration; //set true on identifier tokens that are the identifier of a declaration
	Type decl_type;

	union{
		f64 f64_val;
		s64 s64_val;
		u64 u64_val;
	};
};

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Abstract Syntax Tree 
enum {
	Expression_NULL,

	Expression_Identifier,
	
	Expression_FunctionCall,
	
	//Special ternary conditional expression type
	Expression_TernaryConditional,
	
	//Types
	Expression_Literal,
	
	//Unary Operators
	Expression_UnaryOpBitComp,
	Expression_UnaryOpLogiNOT,
	Expression_UnaryOpNegate,
	Expression_IncrementPrefix,
	Expression_IncrementPostfix,
	Expression_DecrementPrefix,
	Expression_DecrementPostfix,
	Expression_Cast,
	//this is a special expression type that is only made by the validator since the validator has no token to attach typename 
	//information with we must use the TypedValue information on Expression instead in this case, the type set on TypedValue 
	//is the type that is being casted to. the reason we dont just also do this on explicit casts in parsing is because 
	//its possible that the user wants to cast to a struct type that has not been parsed yet and therefore doesnt exist for us 
	//to point to yet. this is also probably beneficial for language servers who read from the validation stage and not the 
	//parser stage, in the case of semantic highlighting, this node would just be ignored
	Expression_CastImplicit, 
	Expression_Reinterpret,

	
	//Binary Operators
	Expression_BinaryOpPlus,
	Expression_BinaryOpMinus,
	Expression_BinaryOpMultiply,
	Expression_BinaryOpDivision,
	Expression_BinaryOpAND,
	Expression_BinaryOpBitAND,
	Expression_BinaryOpOR,
	Expression_BinaryOpBitOR,
	Expression_BinaryOpLessThan,
	Expression_BinaryOpGreaterThan,
	Expression_BinaryOpLessThanOrEqual,
	Expression_BinaryOpGreaterThanOrEqual,
	Expression_BinaryOpEqual,
	Expression_BinaryOpNotEqual,
	Expression_BinaryOpModulo,
	Expression_BinaryOpBitXOR,
	Expression_BinaryOpBitShiftLeft,
	Expression_BinaryOpBitShiftRight,
	Expression_BinaryOpAssignment,
	Expression_BinaryOpMemberAccess,
	Expression_BinaryOpAs,
};

static const char* ExTypeStrings[] = {
	"idLHS: ",
	"idRHS: ",
	
	"fcall: ",
	
	"tern: ",
	
	"literal: ",
	
	"~",
	"!",
	"-",
	"++ pre",
	"++ post",
	"-- pre",
	"-- post",
	
	"+",
	"-",
	"*",
	"/",
	"&&",
	"&",
	"||",
	"|",
	"<",
	">",
	"<=",
	">=",
	"==",
	"!=",
	"%",
	"^",
	"<<",
	">>",
	"=",
	"accessor",
};

Type binop_token_to_expression(Type in){
	switch(in){
		case Token_Multiplication:     return Expression_BinaryOpMultiply;
		case Token_Division:           return Expression_BinaryOpDivision;
		case Token_Negation:           return Expression_BinaryOpMinus;
		case Token_Plus:               return Expression_BinaryOpPlus;
		case Token_AND:                return Expression_BinaryOpAND;
		case Token_OR:                 return Expression_BinaryOpOR;
		case Token_LessThan:           return Expression_BinaryOpLessThan;
		case Token_GreaterThan:        return Expression_BinaryOpGreaterThan;
		case Token_LessThanOrEqual:    return Expression_BinaryOpLessThanOrEqual;
		case Token_GreaterThanOrEqual: return Expression_BinaryOpGreaterThanOrEqual;
		case Token_Equal:              return Expression_BinaryOpEqual;
		case Token_NotEqual:           return Expression_BinaryOpNotEqual;
		case Token_BitAND:             return Expression_BinaryOpBitAND;
		case Token_BitOR:              return Expression_BinaryOpBitOR;
		case Token_BitXOR:             return Expression_BinaryOpBitXOR;
		case Token_BitShiftLeft:       return Expression_BinaryOpBitShiftLeft;
		case Token_BitShiftRight:      return Expression_BinaryOpBitShiftRight;
		case Token_Modulo:             return Expression_BinaryOpModulo;
		case Token_BitNOT:             return Expression_UnaryOpBitComp;
		case Token_LogicalNOT:         return Expression_UnaryOpLogiNOT;
	}
	return Expression_NULL;
}

//held by anything that can represent a value at compile time, currently variables and expressions
//this helps with doing compile time evaluations between variables and expressions
struct Struct;
struct Declaration;
struct TypedValue{
	Type type;
	u32 pointer_depth;
	b32 implicit = 0; //special indicator for variables that were declared with no type and depend on the expression they are assigned
	b32 modifiable = 0;
	//set to a declaration that defines a conversion between this type and another
	Declaration* convertion = 0; 
	Struct* struct_type;
	str8 type_name;
	union {
		f32  float32;
		f64  float64;
		s8   int8;
		s16  int16;
		s32  int32;
		s64  int64;
		u8   uint8;
		u16  uint16;
		u32  uint32;
		u64  uint64;
		str8 str;
	};
};

struct Expression {
	suNode node;
	Token* token_start;
	Token* token_end;
	
	Type type;

	TypedValue data;
};
#define ExpressionFromNode(x) CastFromMember(Expression, node, x)

enum {
	Statement_Unknown,
	Statement_Return,
	Statement_Expression,
	Statement_Declaration,
	Statement_Conditional,
	Statement_Else,
	Statement_For,
	Statement_While,
	Statement_Break,
	Statement_Continue,
	Statement_Struct,
	Statement_Using,
	Statement_Import,
};

struct Statement {
	suNode node;
	Token* token_start;
	Token* token_end;
	
	Type type = Statement_Unknown;
};
#define StatementFromNode(x) CastFromMember(Statement, node, x)

struct Scope {
	suNode node;
	Token* token_start;
	Token* token_end;
	
	b32 has_return_statement = false;
};
#define ScopeFromNode(x) CastFromMember(Scope, node, x)

//represents information about an original declaration and is stored on 
//Struct,Function,and Variable.
//TODO(sushi) this setup is kind of scuffed, maybe just store the 3 structs on decl in a union so we can avoid having 2 layers of casting (eg. VariableFromDeclaration)
struct Struct;
struct Variable;
struct Function;
struct ParserThread;
struct Declaration{
	suNode node;
	Token* token_start;
	Token* token_end;
	
	//name of declaration  
	str8 identifier;
	//name of declaration as it was when it was first declared
	str8 declared_identifier;
	Type type;

	//set true when validator has validated this declaration and all of its child nodes 
	b32 validated = 0;
};

#define DeclarationFromNode(x) CastFromMember(Declaration, node, x)

struct Function {
	Declaration decl;

	Type data_type;
	Struct* struct_data;
	//this label is how a function is internally referred to in the case of overloading
	//it is where name mangling happens
	//format:
	//    func_name@argtype1,argtype2,...@rettype1,rettype2,...
	str8 internal_label;
	u32 positional_args = 0;
	map<str8, Declaration*> args;
	//TODO do this with a binary tree sort of thing instead later
	array<Function*> overloads;
};
#define FunctionFromDeclaration(x) CastFromMember(Function, decl, x)
#define FunctionFromNode(x) FunctionFromDeclaration(DeclarationFromNode(x))

struct Variable{
	Declaration decl;

	//number of times * appears on a variable's type specifier
	u32 pointer_depth;

	TypedValue data;
};
#define VariableFromDeclaration(x) CastFromMember(Variable, decl, x)
#define VariableFromNode(x) VariableFromDeclaration(DeclarationFromNode(x))

struct Struct {
	Declaration decl;
	u64 size; //size of struct in bytes

	Type type;
	
	declmap members;

	//map of conversions defined for this structure
	//the key is the name of the declaration it is a conversion to
	declmap conversions;
};
#define StructFromDeclaration(x) CastFromMember(Struct, decl, x)
#define StructFromNode(x) StructFromDeclaration(DeclarationFromNode(x))


enum{
	Declaration_Unknown,
	Declaration_Function,
	Declaration_Variable,
	Declaration_Structure,
};

struct Program {
	Node node;
	str8 filename;
	//TODO add entrypoint string
};

enum {
	psFile,
	psDirective,
	psImport,
	psRun,
	psScope,
	psDeclaration,
	psStatement,
	psExpression,
	psConditional,
	psLogicalOR,  
	psLogicalAND, 
	psBitwiseOR,  
	psBitwiseXOR, 
	psBitwiseAND, 
	psEquality,   
	psRelational, 
	psBitshift,   
	psAdditive,   
	psTerm,       
	psAccess, 
	psFactor,    
};

const str8 psStrs[] = {
	STR8("File"),
	STR8("Directive"),
	STR8("Import"),
	STR8("Run"),
	STR8("Scope"),
	STR8("Declaration"),
	STR8("Statement"),
	STR8("Expression"),
	STR8("Conditional"),
	STR8("LogicalOR"),  
	STR8("LogicalAND"), 
	STR8("BitwiseOR"),  
	STR8("BitwiseXOR"), 
	STR8("BitwiseAND"), 
	STR8("Equality"),   
	STR8("Relational"), 
	STR8("Bitshift"),   
	STR8("Additive"),   
	STR8("Term"),       
	STR8("Access"),
	STR8("Factor"),     
};

enum{
	FileStage_Null,
	FileStage_Lexer,
	FileStage_Preprocessor,
	FileStage_Parser,
	FileStage_Validator,
};

struct Token;
struct suFile;
struct suMessage{
	u32 verbosity;
	Token* token;
	Type type; //log, error, or warn	
	u32 indent;
	f64 time_made;
	str8 prefix;
	suFile* sufile;
	array<str8> message_parts;
};

struct suLogger{
	// instead of immediatly logging messages we collect them to format and display later
	// we do this to prevent threads' messages from overlapping each other, to prevent
	// actually printing anything during compiling, and so we can do much nicer formatting later on
	// this behavoir is disabled by globals.log_immediatly
	//TODO(sushi) implement an arena instead of array so that we know this will be thread safe
	array<suMessage> messages;

	suFile* sufile = 0;
	str8 owner_str_if_sufile_is_0 = {0,0};
	
	template<typename...T>
	void log(u32 verbosity, T... args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.verbosity < verbosity) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, (sufile ? sufile->file->name : owner_str_if_sufile_is_0), VTS_Default, ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Log;
			message.verbosity = verbosity;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}

	template<typename...T>
	void log(Token* t, u32 verbosity, T... args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.verbosity < verbosity) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, t->file,  VTS_Default, "(",t->l0,",",t->c0,"): ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Log;
			message.verbosity = verbosity;
			message.token = t;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}

	template<typename...T>
	void error(Token* token, T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, token->file, VTS_Default, "(",token->l0,",",token->c0,"): ", ErrorFormat("error"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Error;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			message.token = token;
			messages.add(message);
		}
		if(globals.assert_compiler_on_error) DebugBreakpoint;
	}

	template<typename...T>
	void error(T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, (sufile ? sufile->file->name : owner_str_if_sufile_is_0), VTS_Default, ": ", ErrorFormat("error"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Error;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
		if(globals.assert_compiler_on_error) DebugBreakpoint;
	}

	template<typename...T>
	void warn(Token* token, T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, token->file, VTS_Default, "(",token->l0,",",token->c0,"): ", WarningFormat("warning"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Warn;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}
	
	template<typename...T>
	void warn(T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, (sufile ? sufile->file->name : owner_str_if_sufile_is_0), VTS_Default, ": ", WarningFormat("warning"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Warn;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}

	template<typename...T>
	void note(Token* token, T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, token->file, VTS_Default, "(",token->l0,",",token->c0,"): ", MagentaFormat("note"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Note;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}
	
	template<typename...T>
	void note(T...args){DPZoneScoped;
		if(globals.supress_messages) return;
		if(globals.log_immediatly){
			compiler.mutexes.log.lock();
			str8 out = to_str8_su(VTS_CyanFg, (sufile ? sufile->file->name : owner_str_if_sufile_is_0), MagenetaFormat("note"), ": ", args...);
			Log("", out);
			compiler.mutexes.log.unlock();
		}else{
			suMessage message;
			message.time_made = peek_stopwatch(compiler.ctime);
			message.type = Message_Note;
			constexpr auto arg_count{sizeof...(T)};
			str8 arr[arg_count] = {to_str8(args, deshi_allocator)...};
			message.message_parts.resize(arg_count);
			memcpy(message.message_parts.data, arr, sizeof(str8)*arg_count);
			messages.add(message);
		}
	}
};

struct suFile{
	File* file;
	str8 file_buffer;
	Type stage;
	//set true if a compiler thread is started for this file
	//to prevent multiple compiler threads from starting on the same file
	b32 being_processed = 0;

	suLogger logger;

	//for files to wait on other files to finish certain stages
	//NOTE(sushi) this may only be necessary for validator
	struct{
		condvar lex;
		condvar preprocess;
		condvar parse;
		condvar validate;
	}cv;

	struct{ // lexer
		suArena<Token> tokens;
		suArena<u32> declarations; // list of : tokens
		suArena<u32> imports;      // list of import tokens
		suArena<u32> internals;    // list of internal tokens
		suArena<u32> runs;         // list of run tokens
	}lexer;

	struct{ // preprocessor
		suArena<suFile*> imported_files;
		suArena<u32> exported_decl;
		suArena<u32> internal_decl;
		suArena<u32> runs;
	
	}preprocessor;

	struct{ // parser
		//node that all top-level declarations attach to 
		suNode base;

		//arrays organizing toplevel declarations and import directives
		suArena<Statement*> import_directives;
		//TODO(sushi) it may not be necessary to have exported and imported in separate arrays anymore
		suArena<Declaration*> exported_decl;
		suArena<Declaration*> imported_decl;
		suArena<Declaration*> internal_decl;
	}parser;

	struct{
		
	}validator;

	void init(){
		lexer.tokens.init();
		lexer.declarations.init();
		lexer.imports.init();
		lexer.internals.init();
		lexer.runs.init();
		preprocessor.imported_files.init();
		preprocessor.exported_decl.init();
		preprocessor.internal_decl.init();
		preprocessor.runs.init();
		parser.import_directives.init();
		parser.exported_decl.init();
		parser.imported_decl.init();
		parser.internal_decl.init();
		parser.base.debug = STR8("base");
		parser.base.lock.init();
		cv.lex.init();
		cv.preprocess.init();
		cv.parse.init();
		cv.validate.init();
	}
};


//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Lexer

struct Lexer {
	suFile* sufile;
	void lex();
};

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Preprocessor

struct Preprocessor {
	suFile* sufile;
	void preprocess();
};

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Parser

struct Parser;
struct ParserThread{
	suFile* sufile;
	
	Parser* parser;
	Type stage; //entry stage

	suNode* node;
	Token* curt;

	b32 is_internal;

	b32 finished=0;
	condvar cv;
	
	Declaration* declare();
	suNode* define(suNode* node, Type stage);
	suNode* parse_import(Token* start);

	template<typename ...args> FORCE_INLINE b32
	curr_match(args... in){DPZoneScoped;
		return (((curt)->type == in) || ...);
	}

	template<typename ...args> FORCE_INLINE b32
	curr_match_group(args... in){DPZoneScoped;
		return (((curt)->group == in) || ...);
	}

	template<typename ...args> FORCE_INLINE b32
	next_match(args... in){DPZoneScoped;
		return (((curt + 1)->type == in) || ...);
	}

	template<typename ...args> FORCE_INLINE b32
	next_match_group(args... in){DPZoneScoped;
		return (((curt + 1)->group == in) || ...);
	}

	template<typename... T>
	suNode* binop_parse(suNode* node, suNode* ret, Type next_stage, T... tokchecks);
};

struct Parser {
	array<ParserThread> threads;

	//map of identifiers to global declarations
	declmap pending_globals;

	
	//file the parser is working in
	suFile* sufile;

	void parse();

	void spawn_parser(ParserThread pt);

	void wait_for_dependency(str8 id);
};

void parse_threaded_stub(void* pthreadinfo){DPZoneScoped;
	SetThreadName("parser thread started.");
	ParserThread* pt = (ParserThread*)pthreadinfo;
	pt->sufile = pt->parser->sufile;
	pt->define(pt->node, pt->stage);
	pt->finished = 1;
	pt->cv.notify_all();
}

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Validator

struct Validator{
	suFile* sufile;

	//keeps track of what element we are currently
	struct{
		Variable*   variable = 0;
		Expression* expression = 0;
		Struct*     structure = 0;
		Scope*      scope = 0;
		Function*   function = 0;
		Statement*  statement = 0;
	}current;	

	struct{
		//stacks of declarations known in current scope
		//this array stores the index of the last declaration that was pushed before a new scope begins
		suArena<u32> known_declarations_scope_begin_offsets;
		suArena<Declaration*> known_declarations;
		//stacks of elements that we are working with
		struct{
			suArena<Scope*>      scopes;
			suArena<Struct*>     structs;
			suArena<Variable*>   variables;
			suArena<Function*>   functions;
			suArena<Expression*> expressions;
			suArena<Statement*>  statements;
		}nested;
	}stacks;

	void init(){DPZoneScoped;
		stacks.nested.scopes.init();
		stacks.nested.structs.init();
		stacks.nested.variables.init();
		stacks.nested.functions.init();
		stacks.nested.expressions.init();
		stacks.nested.statements.init();
		stacks.known_declarations.init();
		stacks.known_declarations_scope_begin_offsets.init();

	}

	//starts the validation stage
	void         start();
	//recursive validator function
	suNode*      validate(suNode* node);
	//checks if a variable is going to conflict with another variable in its scope
	//return false if the variable conflicts with another
	b32          check_shadowing(Declaration* d);
	//finds a declaration by iterating the known declarations array backwards
	Declaration* find_decl(str8 id);
	Declaration* find_typename(str8 id);
	//checks if a conversion between 2 types is possible
	b32 can_type_convert(TypedValue* tv0, TypedValue* tv1);
};

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Compiler

struct CompilerRequest{
	Type stage;
	array<str8> filepaths;
};

struct Compiler{

	Stopwatch ctime;

	suLogger logger;

	//TODO(sushi) we probably want to just chunk arenas for suFiles instead of randomly allocating them.
	map<str8, suFile*> files;
	
	//a map of a pair of types and the process used to convert them
	//the conversion is from the first item of pair to the second
	//


	//locked when doing non-thread safe stuff 
	//such as loading a File, and probably when we use memory functions as well
	struct{
		mutex lexer;
		mutex preprocessor;
		mutex parser;
		mutex log; //lock when using logger
		mutex compile_request;
	}mutexes;

	//returns a carray of the suFiles created by the request
	suArena<suFile*> compile(CompilerRequest* request, b32 wait = 1);

	suFile* start_lexer       (suFile* sufile);
	suFile* start_preprocessor(suFile* sufile);
	suFile* start_parser      (suFile* sufile);
	suFile* start_validator   (suFile* sufile);

	//used to completely reset all compiled information
	//this is mainly for performance testing, like running a compile on the same file
	//repeatedly to get an average time
	//or to test for memory leaks
	void reset(); 

}compiler;

struct CompilerThread{
	str8 filepath;
	Type stage;
	condvar wait;
	b32 finished;
	suFile* sufile;
};

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Helpers

str8 type_token_to_str(Type type){
    switch(type){
        case Token_Void:       return STR8("void");
        case Token_Signed8:    return STR8("s8");
        case Token_Signed16:   return STR8("s16");
        case Token_Signed32:   return STR8("s32");
        case Token_Signed64:   return STR8("s64");
        case Token_Unsigned8:  return STR8("u8");
        case Token_Unsigned16: return STR8("u16");
        case Token_Unsigned32: return STR8("u32");
        case Token_Unsigned64: return STR8("u64");
        case Token_Float32:    return STR8("f32");
        case Token_Float64:    return STR8("f64");
        case Token_String:     return STR8("str");
        case Token_Any:        return STR8("any");
        case Token_Struct:     return STR8("struct-type");
    }
    return STR8("UNKNOWN DATA TYPE");
}

str8 get_typename(Variable* v){
	if(v->data.type == Token_Struct){
		return v->data.struct_type->decl.identifier;
	}else{
		return type_token_to_str(v->data.type);
	}
}

str8 get_typename(Expression* e){
	if(e->data.type == Token_Struct){
		return e->data.struct_type->decl.identifier;
	}else{
		return type_token_to_str(e->data.type);
	}
}

str8 get_typename(Struct* s){
	return s->decl.identifier;
}

//this overload may be unecessary
str8 get_typename(TNode* n){
	switch(n->type){
		case NodeType_Variable:   return get_typename(VariableFromNode(n));
		case NodeType_Expression: return get_typename(ExpressionFromNode(n));
		case NodeType_Structure:  return get_typename(StructFromNode(n));
		default: Assert(false,"Invalid node type given to get_typename()"); return {0};
	}
	return {0};
}



u64 builtin_sizes(Type type){
	switch(type){
		case Token_Unsigned8:  return sizeof(u8);
		case Token_Unsigned16: return sizeof(u16);
		case Token_Unsigned32: return sizeof(u32);
		case Token_Unsigned64: return sizeof(u64);
		case Token_Signed8:    return sizeof(s8);
		case Token_Signed16:   return sizeof(s16);
		case Token_Signed32:   return sizeof(s32);
		case Token_Signed64:   return sizeof(s64);
		case Token_Float32:    return sizeof(f32);
		case Token_Float64:    return sizeof(f64);
		case Token_String:     return sizeof(void*) + sizeof(u64);
		case Token_Any:        return sizeof(void*);
	}
	return -1;
}

//~////////////////////////////////////////////////////////////////////////////////////////////////
//// Memory

struct{
	suChunkedArena<Function>   functions;
	suChunkedArena<Variable>   variables;
	suChunkedArena<Struct>     structs;
	suChunkedArena<Scope>      scopes;
	suChunkedArena<Expression> expressions;
	suChunkedArena<Statement>  statements;

	//TODO(sushi) bypass debug message assignment in release build
	FORCE_INLINE
	Function* make_function(str8 debugmsg = STR8("")){DPZoneScoped;
		Function* function = functions.add(Function());
		compiler.logger.log(Verbosity_Debug, "Making a function with debug message ", debugmsg);
		function->decl.node.lock.init();
		function->decl.node.type = NodeType_Function;
		function->decl.node.debug = debugmsg;
		return function;
	}

	FORCE_INLINE
	Variable* make_variable(str8 debugmsg = STR8("")){DPZoneScoped;
		Variable* variable = variables.add(Variable());
		compiler.logger.log(Verbosity_Debug, "Making a variable with debug message ", debugmsg);
		variable->decl.node.lock.init();
		variable->decl.node.type = NodeType_Variable;
		variable->decl.node.debug = debugmsg;
		return variable;
	}

	FORCE_INLINE
	Struct* make_struct(str8 debugmsg = STR8("")){DPZoneScoped;
		Struct* structure = structs.add(Struct());
		compiler.logger.log(Verbosity_Debug, "Making a structure with debug message ", debugmsg);
		structure->decl.node.lock.init();
		structure->decl.node.type = NodeType_Structure;
		structure->decl.node.debug = debugmsg;
		return structure;
	}

	FORCE_INLINE
	Scope* make_scope(str8 debugmsg = STR8("")){DPZoneScoped;
		Scope* scope = scopes.add(Scope());
		compiler.logger.log(Verbosity_Debug, "Making a scope with debug message ", debugmsg);
		scope->node.lock.init();
		scope->node.type = NodeType_Scope;
		scope->node.debug = debugmsg;
		return scope;
	}

	FORCE_INLINE
	Expression* make_expression(str8 debugmsg = STR8("")){DPZoneScoped;
		Expression* expression = expressions.add(Expression());
		compiler.logger.log(Verbosity_Debug, "Making an expression with debug message ", debugmsg);
		expression->node.lock.init();
		expression->node.type = NodeType_Expression;
		expression->node.debug = debugmsg;
		return expression;
	}

	FORCE_INLINE
	Statement* make_statement(str8 debugmsg = STR8("")){DPZoneScoped;
		Statement* statement = statements.add(Statement());
		compiler.logger.log(Verbosity_Debug, "Making a statement with debug message ", debugmsg);
		statement->node.lock.init();
		statement->node.type = NodeType_Statement;
		statement->node.debug = debugmsg;
		return statement;
	}

	FORCE_INLINE
	void init(){DPZoneScoped;
		functions.init(256);   
		variables.init(256);   
		structs.init(256);     
		scopes.init(256);      
		expressions.init(256); 
		statements.init(256);
	}

}arena;

#endif //SU_TYPES_H