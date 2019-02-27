#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"


extern uns64 SWP_CORE0_WAYS; // Input Way partitions for Core 0       
extern uns64 cycle; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
  Flag outcome=MISS;

  // Your Code Goes Here
  uns64 ind,tag_part,i;
  ind=(lineaddr) & (c->num_sets-1);
  //ind=(int)log2(ind_bits); //ind = log to the base 2 of ind_bits
  tag_part=lineaddr;
  if(is_write == 1)
  {
    c->stat_write_access++;
  }
  else
  {
    c->stat_read_access++;
  }

  for(i=0;i<c->num_ways;i++)
  {
    if(c->sets[ind].line[i].valid && c->sets[ind].line[i].core_id==core_id)
    {
      if(c->sets[ind].line[i].tag == tag_part)
      {
        outcome=HIT;
        c->sets[ind].line[i].last_access_time=cycle;
        if(is_write==1)
        {
          c->sets[ind].line[i].dirty=1;
        }
        break;
      }
    }
  }

  if((outcome==MISS) && (is_write==1))
    {c->stat_write_miss++;}
  if((outcome==MISS) && (is_write==0))
    {c->stat_read_miss++;}

  return outcome;

}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){

  // Your Code Goes Here
  // Find victim using cache_find_victim
  // Initialize the evicted entry
  // Initialize the victime entry
  uns64 i,flag=0,ind,tag_part;
  uns vic;

  ind=(lineaddr) & (c->num_sets-1);
  tag_part=lineaddr;

// if(c->repl_policy==0 || c->repl_policy==1)  //comment here
// {  //Find space. If not empty, find victim
    for(i=0;i<c->num_ways;i++)
    {
      if(c->sets[ind].line[i].valid==0)
      {
        flag=1;
        c->sets[ind].line[i].valid=1;
      //c->sets[ind].line[i].dirty=0;
        c->sets[ind].line[i].tag=tag_part;
        c->sets[ind].line[i].core_id=core_id;
        c->sets[ind].line[i].last_access_time=cycle;

        if(is_write)
        {
          c->sets[ind].line[i].dirty=1;
        } 
        else
        {
          c->sets[ind].line[i].dirty=0;
        }

        break;
      }
    }

    if(flag==0)
    {
      vic=cache_find_victim(c,ind,core_id);

      c->last_evicted_line=c->sets[ind].line[vic]; 

      if(c->sets[ind].line[vic].dirty==1)
      {
        c->stat_dirty_evicts++;
      }
   
      c->sets[ind].line[vic].valid=1;

      if(is_write)
      {
        c->sets[ind].line[vic].dirty=1;
      } 
      else
      {
        c->sets[ind].line[vic].dirty=0;
      }

      c->sets[ind].line[vic].core_id=core_id;
      c->sets[ind].line[vic].tag=tag_part;
      c->sets[ind].line[vic].last_access_time=cycle;
    }
} 


////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////


uns cache_find_victim(Cache *c, uns set_index, uns core_id){
  uns victim=0;

  // TODO: Write this using a switch case statement
  uns min;
  uns64 i;
  
  if (c->repl_policy==1)  //random - rand function
  {
    victim = rand() % (c->num_ways);
  }

  else if (c->repl_policy==0) //LRU for non-partitioned cache
  { 
    min=c->sets[set_index].line[0].last_access_time;
    for(i=0;i<c->num_ways;i++)
    { 
      if(c->sets[set_index].line[i].last_access_time < min)
      {
        min=c->sets[set_index].line[i].last_access_time;
        victim=i;
      }
    }
  }

  else if (c->repl_policy==2)  //LRU for partitioned cache
  { 
    //printf("SWP L2 victim\n");
     victim=0;

//OLD METHOD
    if(core_id==0)
    {
    min=c->sets[set_index].line[0].last_access_time;

    for(i=0;i<SWP_CORE0_WAYS;i++)
    { 
      if(c->sets[set_index].line[i].last_access_time < min)
      {
        min=c->sets[set_index].line[i].last_access_time;
        victim=i;
        //printf("Victim 0: %d \n",victim);
      }
    }
    }
    
    else if(core_id==1)
    {
    min=c->sets[set_index].line[SWP_CORE0_WAYS].last_access_time;
    victim=SWP_CORE0_WAYS;
    for(i=SWP_CORE0_WAYS;i<c->num_ways;i++)
    { 
      if(c->sets[set_index].line[i].last_access_time < min)
      {
        min=c->sets[set_index].line[i].last_access_time;
        victim=i;
        //printf("Victim 1: %d \n",victim);
      }
    }
    }
  }

  return victim;
}

