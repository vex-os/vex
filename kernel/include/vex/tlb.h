#pragma once

/// @pre The page tables are already changed
/// @post No hart can reach the old translations through its TLB
void tlb_shootdown(void);

void tlb_poll(void);
void tlb_hart_online(void);
