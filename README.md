# Dynamic Memory Allocation with Weight Balanced Heap Tree
These are two variants of Brent's algorithm.[1] The differences are as below.
1. Eliminating additional tree utilizing payload area.
2. The tree grows or shrinks dynamically, always keeping balance using weight of sub trees.
2. Eliminating linked list, using paging.
3. A multi-thread implementation.
The time complexity is O(log n).

# Internal Fragmentation vs External Fragmentation
External fragmentation is chosen over internal fragmentation, because external fragmentation can become available again upon coalesing, while internal fragmentation will not be available until the object is freed.

# Benchmark Results as of v1.0.0

MacBookPro 16-inch, 2019/Parellels Desktop 20/Ubuntu 22.04.5 LTS/mimalloc-bench.[2]
1. Processors 4
2. Adaptive Hypervisor (Apple)
wbht	Weight Balanced Heap Tree
avlht	AVL Heap Tree

 test    alloc   time  rss    user  sys  page-faults page-reclaims<br>
cfrac       sys   05.73 3072 5.72 0.00 1 424<br>
cfrac       dh    15.63 7552 15.61 0.00 8 775<br>
cfrac       ff    08.38 250028 7.27 1.09 0 669375<br>
cfrac       hd    05.50 5248 5.50 0.00 0 544<br>
cfrac       hm    10.25 7296 10.24 0.00 0 1096<br>
cfrac       hml   07.40 4864 7.39 0.01 0 560<br>
cfrac       iso   10.35 34432 10.23 0.11 0 8786<br>
cfrac       je    05.34 5376 5.34 0.00 0 485<br>
cfrac       lf    06.63 4224 6.62 0.00 1 689<br>
cfrac       lp    05.72 3456 5.70 0.01 8 446<br>
cfrac       lt    05.46 4992 5.46 0.00 1 501<br>
cfrac       mi    05.27 3072 5.25 0.00 1 374<br>
cfrac       mi-sec 06.78 3712 6.77 0.00 1 507<br>
cfrac       mi2   05.26 3200 5.26 0.00 1 386<br>
cfrac       mi2-sec 07.03 3584 7.03 0.00 1 504<br>
cfrac       mng   07.81 2560 7.78 0.02 1 7294<br>
cfrac       mesh  05.74 3200 5.73 0.00 2 909<br>
cfrac       rp    05.24 3840 5.23 0.00 1 739<br>
cfrac       scudo 07.20 4992 7.19 0.00 1 611<br>
cfrac       sg    05.69 3072 5.68 0.00 1 431<br>
cfrac       sm    08.15 5632 8.14 0.00 1 678<br>
cfrac       sn    05.17 3200 5.16 0.00 1 413<br>
cfrac       sn-sec 05.43 3456 5.43 0.00 2 489<br>
cfrac       tbb   06.78 5120 6.77 0.00 2 519<br>
cfrac       tc    05.30 8704 5.29 0.00 2 1378<br>
cfrac       yal   07.98 2944 7.97 0.00 1 436<br>
cfrac       wbht  07.37 3200 7.25 0.11 1 16616<br>
espresso    sys   05.06 2432 5.03 0.02 2 462<br>
espresso    dh    08.59 8064 8.57 0.01 0 862<br>
espresso    ff    06.88 41984 5.36 1.48 0 944662<br>
espresso    hd    04.54 5888 4.52 0.01 0 690<br>
espresso    hm    06.88 22144 6.84 0.04 0 8520<br>
espresso    hml   05.43 5504 5.28 0.14 0 18207<br>
espresso    iso   06.91 69732 6.75 0.14 0 66918<br>
espresso    je    04.52 5632 4.50 0.01 0 589<br>
espresso    lf    04.99 7040 4.97 0.01 0 1475<br>
espresso    lp    05.04 2944 5.02 0.01 0 486<br>
espresso    lt    04.47 6144 4.46 0.00 0 790<br>
espresso    mi    04.47 3352 4.45 0.01 0 3659<br>
espresso    mi-sec 05.09 5464 5.06 0.02 0 10452<br>
espresso    mi2   04.41 3840 4.39 0.01 0 642<br>
espresso    mi2-sec 05.11 6656 5.09 0.02 0 1336<br>
espresso    mng   09.14 2528 6.43 2.61 0 749782<br>
espresso    mesh  04.75 3248 4.71 0.02 0 2127<br>
espresso    rp    04.43 8064 4.42 0.01 0 1779<br>
espresso    scudo 05.43 5376 5.41 0.01 0 652<br>
espresso    sg    05.08 2432 5.05 0.02 0 467<br>
espresso    sm    07.08 6272 7.06 0.02 0 824<br>
espresso    sn    05.32 4224 5.29 0.02 0 667<br>
espresso    sn-sec 04.63 12416 4.61 0.01 0 2739<br>
espresso    tbb   05.38 5248 5.36 0.01 0 548<br>
espresso    tc    04.71 9216 4.69 0.01 0 1532<br>
espresso    yal   06.00 4480 5.99 0.01 0 994<br>
espresso    wbht  05.85 2300 5.82 0.03 0 7608<br>
barnes      sys   02.71 58368 2.68 0.02 1 16563<br>
barnes      dh    02.77 65024 2.74 0.02 0 17428<br>
barnes      ff    02.72 95232 2.68 0.03 0 25824<br>
barnes      hd    02.82 60416 2.79 0.03 0 16711<br>
barnes      hm    02.73 61056 2.70 0.02 0 16863<br>
barnes      hml   02.65 60288 2.63 0.01 0 16673<br>
barnes      iso   02.58 82944 2.55 0.03 0 22774<br>
barnes      je    02.55 60800 2.52 0.02 0 16715<br>
barnes      lf    02.57 59520 2.54 0.02 0 16896<br>
barnes      lp    02.58 58496 2.55 0.02 0 16583<br>
barnes      lt    02.53 60288 2.50 0.02 0 16659<br>
barnes      mi    02.56 58368 2.53 0.02 0 16579<br>
barnes      mi-sec 02.55 58496 2.53 0.02 0 16600<br>
barnes      mi2   02.54 58496 2.51 0.02 0 16613<br>
barnes      mi2-sec 02.52 58624 2.50 0.02 0 16637<br>
barnes      mng   02.55 58240 2.53 0.02 0 16574<br>
barnes      mesh  02.59 59008 2.56 0.02 0 14317<br>
barnes      rp    02.59 59392 2.56 0.02 0 16925<br>
barnes      scudo 02.52 60160 2.50 0.02 0 16642<br>
barnes      sg    02.54 58240 2.51 0.02 0 16566<br>
barnes      sm    02.50 66636 2.49 0.01 0 2765<br>
barnes      sn    02.56 59008 2.53 0.02 0 16914<br>
barnes      sn-sec 02.53 59264 2.50 0.02 0 16932<br>
barnes      tbb   02.57 60672 2.54 0.02 0 16702<br>
barnes      tc    02.58 64000 2.56 0.02 0 17562<br>
barnes      yal   02.54 58496 2.52 0.02 0 16606<br>
barnes      wbht  02.53 58240 2.50 0.02 0 16570<br>
leanN       sys   16.53 214120 33.98 1.02 0 52234<br>
leanN       dh    3:43.78 460676 238.37 190.47 0 325732<br>
leanN       ff    40.93 1461768 43.00 13.73 89543 2330930<br>
leanN       hd    16.10 232352 33.48 1.02 33 59950<br>
Command exited with non-zero status 1<br>
leanN       hm    09.03 214880 16.12 1.77 3 75156<br>
leanN       hml   17.44 219856 33.14 3.17 1 172027<br>
leanN       iso   2:51.05 875640 183.51 78.44 1 260215<br>
leanN       je    13.39 192816 27.12 1.15 3 57184<br>
leanN       lf    15.59 214248 31.84 1.39 1 95132<br>
leanN       lp    16.42 214144 33.40 0.98 8 52560<br>
Command terminated by signal 11<br>
leanN       lt    11.09 158216 22.42 1.39 1 179538<br>
leanN       mi    13.54 193232 27.76 1.01 1 78465<br>
leanN       mi-sec 15.25 259348 30.95 1.26 1 136092<br>
leanN       mi2   13.66 200228 28.22 0.98 1 72850<br>
leanN       mi2-sec 15.37 237252 31.38 1.09 1 98173<br>
leanN       mng   44.07 212272 76.75 29.86 1 445774<br>
leanN       mesh  19.07 203488 36.39 3.40 2 110463<br>
leanN       rp    13.00 245120 26.33 0.95 1 58481<br>
leanN       scudo 17.57 270528 36.26 1.23 1 88459<br>
leanN       sg    16.64 215272 34.30 1.02 1 52647<br>
Command terminated by signal 6<br>
leanN       sm    17.53 205968 36.50 1.14 1 58547<br>
leanN       sn    12.68 213632 25.77 0.93 1 50589<br>
leanN       sn-sec 13.79 278456 28.52 0.90 2 66749<br>
leanN       tbb   14.26 254672 28.95 1.18 2 77945<br>
leanN       tc    23.72 191872 47.95 1.35 2 45107<br>
leanN       yal   28.14 279048 60.78 1.74 1 88044<br>
leanN       wbht  35.07 286788 72.25 1.31 1 139359<br>
larsonN-sized sys   19.784 36608 19.62 0.09 2 8370<br>
larsonN-sized dh    2253.915 30928 5.56 5.81 8 7225<br>
larsonN-sized ff    881.561 131284 1.25 7.21 0 798607<br>
larsonN-sized hd    17.459 29440 19.66 0.08 0 6607<br>
larsonN-sized hm    132.127 54556 18.23 0.89 0 36952<br>
larsonN-sized hml   47.660 23552 18.17 0.90 0 59212<br>
larsonN-sized iso   934.776 72064 6.02 5.82 0 17304<br>
larsonN-sized je    13.866 37228 19.37 0.31 0 23245<br>
larsonN-sized lf    35.206 101632 19.38 0.17 0 24575<br>
larsonN-sized lp    21.182 33664 19.62 0.09 0 7575<br>
larsonN-sized lt    17.894 29440 19.63 0.08 0 6776<br>
larsonN-sized mi    14.387 43112 19.57 0.10 0 11027<br>
larsonN-sized mi-sec 60.794 43128 18.60 0.43 0 44785<br>
larsonN-sized mi2   13.981 53400 19.51 0.13 0 24931<br>
larsonN-sized mi2-sec 56.454 41948 19.23 0.16 0 26663<br>
larsonN-sized mng   320.336 16640 10.95 5.95 0 4568<br>
larsonN-sized mesh  170.735 22400 9.67 4.38 0 6207<br>
larsonN-sized rp    19.877 40064 19.52 0.10 0 9453<br>
larsonN-sized scudo 50.715 20736 18.86 0.29 0 4505<br>
larsonN-sized sg    34.333 32384 19.42 0.08 0 7338<br>
larsonN-sized sm    29.011 23552 19.56 0.07 0 5142<br>
larsonN-sized sn    25.621 48256 19.55 0.10 0 11298<br>
larsonN-sized sn-sec 27.594 51584 19.53 0.10 0 12133<br>
larsonN-sized tbb   40.630 41984 19.28 0.09 0 11940<br>
larsonN-sized tc    26.607 26496 19.40 0.12 0 5789<br>
larsonN-sized yal   53.401 23464 19.19 0.11 0 7910<br>
larsonN-sized wbht  139.120 83016 19.51 0.07 0 20986<br>
mstressN    sys   00.53 81576 0.97 0.16 0 29914<br>
mstressN    dh    05.02 58088 4.43 4.91 0 231437<br>
mstressN    ff    01.72 82436 1.00 1.97 0 376414<br>
mstressN    hd    00.56 52680 1.05 0.22 0 41454<br>
mstressN    hm    01.70 84640 1.81 1.91 0 279501<br>
mstressN    hml   01.59 46392 1.32 1.94 0 290226<br>
mstressN    iso   01.99 145240 2.55 1.67 0 37305<br>
mstressN    je    01.06 49260 1.12 1.12 0 268916<br>
mstressN    lf    00.95 236244 1.02 0.88 0 254086<br>
mstressN    lp    00.53 78720 0.99 0.14 0 27135<br>
mstressN    lt    00.91 42988 1.01 0.90 0 237924<br>
mstressN    mi    00.48 125540 0.91 0.14 0 36559<br>
mstressN    mi-sec 00.71 142612 1.09 0.55 0 45211<br>
mstressN    mi2   00.44 104008 0.79 0.19 0 28076<br>
mstressN    mi2-sec 00.52 105148 0.99 0.17 0 31053<br>
mstressN    mng   02.91 40244 1.91 3.84 0 314861<br>
mstressN    mesh  00.84 56724 1.03 0.50 0 90771<br>
mstressN    rp    00.53 72896 0.75 0.22 0 78028<br>
mstressN    scudo 00.81 42860 1.16 0.53 0 164680<br>
mstressN    sg    00.53 78616 1.00 0.14 0 31080<br>
mstressN    sm    00.71 65412 1.21 0.41 0 98750<br>
mstressN    sn    00.47 68352 0.89 0.12 0 16742<br>
mstressN    sn-sec 00.54 87552 0.99 0.20 0 21585<br>
mstressN    tbb   00.45 50220 0.86 0.13 0 14941<br>
mstressN    tc    00.46 55936 0.89 0.11 0 13205<br>
mstressN    yal   00.88 126824 1.34 0.53 0 152720<br>
mstressN    wbht  01.23 44896 1.37 1.10 0 264696<br>
gs          sys   02.59 40560 2.37 0.11 139 7377<br>
gs          dh    03.97 67784 3.33 0.62 0 84430<br>
gs          ff    05.40 126788 2.91 2.45 0 1035758<br>
gs          hd    02.50 46080 2.42 0.06 0 7987<br>
gs          hm    04.29 45540 3.22 1.01 0 92196<br>
gs          hml   04.10 40620 3.09 0.96 0 90432<br>
gs          iso   03.01 99584 2.83 0.18 0 21339<br>
gs          je    02.48 47488 2.42 0.06 0 8233<br>
gs          lf    03.61 42768 2.87 0.71 0 76766<br>
gs          lp    02.43 41396 2.37 0.06 0 7469<br>
gs          lt    03.43 43332 2.79 0.62 0 77150<br>
gs          mi    02.42 41344 2.36 0.05 0 7066<br>
gs          mi-sec 02.48 45568 2.40 0.07 0 8186<br>
gs          mi2   02.46 43888 2.40 0.05 0 7592<br>
gs          mi2-sec 02.50 45332 2.43 0.06 0 8206<br>
gs          mng   03.54 41400 2.82 0.70 0 76175<br>
gs          mesh  02.51 41820 2.40 0.11 0 18575<br>
gs          rp    02.45 43904 2.39 0.06 0 7805<br>
gs          scudo 02.49 41704 2.39 0.10 0 17151<br>
gs          sg    02.43 40900 2.37 0.06 0 7445<br>
gs          sm    02.38 48128 2.32 0.05 0 8802<br>
gs          sn    02.47 44544 2.40 0.06 0 7572<br>
gs          sn-sec 02.54 50176 2.47 0.07 0 8897<br>
gs          tbb   02.47 46080 2.39 0.07 0 7906<br>
gs          tc    02.45 46464 2.39 0.05 0 8032<br>
gs          yal   02.48 42336 2.42 0.05 0 7561<br>
gs          wbht  03.89 42748 2.93 0.91 0 99058<br>
lua         sys   11.25 61456 9.83 1.09 551 144337<br>
Command exited with non-zero status 2<br>
lua         dh    00.02 12672 0.01 0.01 5 1593<br>
lua         ff    08.81 112600 5.68 3.09 0 1830385<br>
lua         hd    06.08 71680 5.41 0.66 0 166912<br>
lua         hm    08.59 84328 6.87 1.71 0 472775<br>
lua         hml   06.73 64468 5.72 1.01 0 257914<br>
lua         iso   09.75 153984 7.22 2.52 0 1142205<br>
lua         je    11.11 69152 10.01 1.09 0 167516<br>
lua         lf    11.64 79444 10.16 1.45 0 223906<br>
lua         lp    11.03 62152 10.00 1.02 0 147117<br>
lua         lt    11.06 66932 9.67 1.37 0 209064<br>
lua         mi    10.56 68416 9.60 0.95 0 152912<br>
lua         mi-sec 11.42 77228 10.21 1.20 0 200466<br>
lua         mi2   10.73 65588 9.72 1.01 0 155697<br>
lua         mi2-sec 11.28 67112 10.13 1.14 0 193233<br>
lua         mng   16.10 60876 11.41 4.59 0 548783<br>
lua         mesh  07.39 64660 6.59 0.79 0 174765<br>
lua         rp    06.03 75136 5.32 0.71 0 202859<br>
lua         scudo 06.23 76428 5.53 0.69 0 163282<br>
lua         sg    06.02 61848 5.45 0.57 0 145083<br>
lua         sm    11.12 74088 9.82 1.30 0 236987<br>
lua         sn    10.78 67860 9.77 1.01 0 160031<br>
lua         sn-sec 10.99 75224 9.78 1.22 0 202881<br>
lua         tbb   11.17 75264 9.98 1.18 0 163868<br>
lua         tc    11.32 74368 9.96 1.35 0 257813<br>
lua         yal   11.55 72440 10.35 1.20 0 178953<br>
lua         wbht  11.59 64176 10.40 1.18 0 174470<br>
alloc-test1 sys   09.70 14080 9.69 0.00 1 2932<br>
alloc-test1 dh    35.11 20464 35.08 0.01 0 4361<br>
alloc-test1 ff    13.97 748796 12.04 1.89 0 411592<br>
alloc-test1 hd    07.74 13952 7.73 0.01 0 2705<br>
alloc-test1 hm    11.55 22656 11.52 0.02 0 6583<br>
alloc-test1 hml   07.09 13696 7.09 0.00 0 2808<br>
alloc-test1 iso   10.41 110976 10.26 0.14 0 28081<br>
alloc-test1 je    04.30 13056 4.29 0.00 0 2523<br>
alloc-test1 lf    05.78 13824 5.77 0.01 0 2854<br>
alloc-test1 lp    09.74 14592 9.72 0.02 0 2957<br>
alloc-test1 lt    08.03 12416 8.02 0.00 0 2521<br>
alloc-test1 mi    07.69 13568 7.69 0.00 0 2580<br>
alloc-test1 mi-sec 10.35 14720 10.34 0.01 0 2901<br>
alloc-test1 mi2   07.58 13440 7.57 0.01 0 2594<br>
alloc-test1 mi2-sec 10.93 14848 10.91 0.01 0 2961<br>
alloc-test1 mng   16.41 13312 16.39 0.01 0 3377<br>
alloc-test1 mesh  14.10 16128 14.07 0.02 0 4637<br>
alloc-test1 rp    08.87 14080 8.86 0.01 0 2991<br>
alloc-test1 scudo 10.76 16128 10.75 0.01 0 3451<br>
alloc-test1 sg    09.72 14208 9.72 0.00 0 2935<br>
alloc-test1 sm    11.99 13444 11.98 0.00 0 1730<br>
alloc-test1 sn    07.81 13952 7.80 0.00 0 2731<br>
alloc-test1 sn-sec 05.04 14592 5.02 0.01 0 2899<br>
alloc-test1 tbb   05.70 13568 5.69 0.00 0 2759<br>
alloc-test1 tc    04.20 16640 4.20 0.00 0 3360<br>
alloc-test1 yal   06.86 13824 6.85 0.00 0 2854<br>
alloc-test1 wbht  08.04 21760 8.02 0.01 0 4806<br>
alloc-testN sys   05.42 14848 21.05 0.02 0 3123<br>
alloc-testN dh    9:06.87 20268 516.40 773.20 0 4381<br>
alloc-testN ff    27.08 1128844 54.15 27.66 0 1587729<br>
alloc-testN hd    07.70 14208 29.68 0.04 0 2797<br>
alloc-testN hm    18.30 47488 69.56 0.60 0 16314<br>
alloc-testN hml   12.50 14592 48.78 0.08 0 3032<br>
alloc-testN iso   3:01.94 82304 264.16 242.69 0 21517<br>
alloc-testN je    07.49 13952 28.92 0.04 0 2801<br>
alloc-testN lf    09.65 16128 37.76 0.05 0 3429<br>
alloc-testN lp    09.09 15232 35.53 0.04 0 3148<br>
alloc-testN lt    07.47 14080 29.20 0.05 0 2998<br>
alloc-testN mi    07.29 14592 28.53 0.03 0 2930<br>
alloc-testN mi-sec 09.92 16768 38.59 0.05 0 3515<br>
alloc-testN mi2   07.31 14336 28.43 0.03 0 2969<br>
alloc-testN mi2-sec 10.13 17408 39.26 0.03 0 3676<br>
alloc-testN mng   2:38.52 13568 279.52 226.69 0 3678<br>
alloc-testN mesh  18.19 16384 50.12 8.42 0 4701<br>
alloc-testN rp    08.08 16000 31.66 0.04 0 3490<br>
alloc-testN scudo 10.57 15744 40.81 0.05 0 5248<br>
alloc-testN sg    09.08 14976 35.50 0.04 0 3126<br>
alloc-testN sm    11.72 16256 45.52 0.10 0 3820<br>
alloc-testN sn    07.53 14976 29.22 0.03 0 3017<br>
alloc-testN sn-sec 07.83 16128 30.28 0.03 0 3295<br>
alloc-testN tbb   10.51 14336 40.59 0.05 0 3050<br>
alloc-testN tc    07.91 17408 30.71 0.02 0 3577<br>
alloc-testN yal   12.41 15488 48.62 0.04 0 3369<br>
alloc-testN wbht  14.00 25728 54.51 0.07 0 5844<br>
sh6benchN   sys   03.92 422912 14.49 0.65 0 105458<br>
sh6benchN   dh    5:34.06 713744 297.41 647.46 0 197753<br>
sh6benchN   ff    16.04 1356572 15.59 20.91 1 1403195<br>
sh6benchN   hd    01.60 356608 5.03 0.89 16 89662<br>
Command exited with non-zero status 1<br>
sh6benchN   hm    05.35 176640 17.81 2.32 1 52735<br>
sh6benchN   hml   05.40 351104 19.07 1.53 1 87066<br>
sh6benchN   iso   1:36.48 1449728 93.79 147.83 12999 598399<br>
sh6benchN   je    01.34 273444 4.58 0.31 22 69291<br>
sh6benchN   lf    02.58 251648 9.69 0.22 1 62666<br>
sh6benchN   lp    03.65 423168 13.50 0.53 8 105481<br>
sh6benchN   lt    01.32 257280 4.27 0.49 1 68964<br>
sh6benchN   mi    00.70 264960 2.46 0.21 1 66372<br>
sh6benchN   mi-sec 02.78 344064 9.54 0.84 1 86369<br>
sh6benchN   mi2   00.68 265856 2.27 0.21 1 66266<br>
sh6benchN   mi2-sec 02.88 346880 10.92 0.32 1 86585<br>
sh6benchN   mng   50.17 349784 51.53 88.92 1 582052<br>
sh6benchN   mesh  03.32 318716 10.53 1.68 2 80658<br>
sh6benchN   rp    00.81 305792 2.82 0.24 1 76261<br>
sh6benchN   scudo 12.58 478976 42.86 2.42 1 179922<br>
sh6benchN   sg    03.63 422784 13.45 0.55 1 105464<br>
sh6benchN   sm    04.32 269312 16.02 0.72 1 68809<br>
sh6benchN   sn    00.74 320384 2.49 0.25 1 79836<br>
sh6benchN   sn-sec 01.17 368128 4.07 0.30 2 91787<br>
sh6benchN   tbb   02.37 302080 8.89 0.26 2 74763<br>
sh6benchN   tc    00.81 271488 2.74 0.26 2 67097<br>
sh6benchN   yal   05.01 292016 19.05 0.35 1 76458<br>
sh6benchN   wbht  03.53 446204 11.38 1.43 1 158140<br>
sh8benchN   sys   10.43 166576 40.14 0.45 0 101688<br>
sh8benchN   dh    10:47.81 173328 549.09 1402.61 8 161304<br>
sh8benchN   ff    52.74 175196 32.03 66.45 1 6022109<br>
sh8benchN   hd    05.72 241956 18.55 3.20 2 94185<br>
sh8benchN   hm    1:02.78 151560 107.56 87.85 1 722846<br>
sh8benchN   hml   1:54.99 132076 72.12 228.57 1 1671612<br>
sh8benchN   iso   6:08.33 336488 344.61 355.72 0 233886<br>
sh8benchN   je    02.23 189840 8.51 0.13 0 51408<br>
sh8benchN   lf    06.43 122636 13.79 3.71 0 146826<br>
sh8benchN   lp    05.47 168232 21.24 0.20 0 106329<br>
Command terminated by signal 11<br>
sh8benchN   lt    03.21 123300 6.91 1.95 0 82331<br>
sh8benchN   mi    01.19 127488 4.47 0.07 0 39481<br>
sh8benchN   mi-sec 09.33 156560 21.82 5.19 0 1050418<br>
sh8benchN   mi2   01.47 156956 5.39 0.08 0 39149<br>
sh8benchN   mi2-sec 05.68 171392 22.02 0.09 0 42560<br>
sh8benchN   mng   54.48 130200 79.93 106.78 0 509064<br>
sh8benchN   mesh  07.43 121980 21.42 4.94 0 150333<br>
sh8benchN   rp    01.39 143104 5.22 0.06 0 35621<br>
sh8benchN   scudo 22.52 136380 84.73 1.63 0 53756<br>
sh8benchN   sg    05.01 165752 19.30 0.24 0 108593<br>
sh8benchN   sm    05.49 147124 21.41 0.18 0 19396<br>
sh8benchN   sn    01.37 183680 5.17 0.10 0 45605<br>
sh8benchN   sn-sec 02.26 171264 8.57 0.07 0 42534<br>
sh8benchN   tbb   03.38 137764 12.73 0.43 0 34202<br>
sh8benchN   tc    06.63 127616 19.38 6.40 0 31122<br>
sh8benchN   yal   06.03 354416 23.20 0.26 0 106180<br>
sh8benchN   wbht  17.36 141348 16.06 22.01 0 4812433<br>
xmalloc-testN sys   3.877 50128 17.36 0.85 1 38764<br>
xmalloc-testN dh    54.401 52152 4.96 11.48 0 13173<br>
xmalloc-testN ff    8.857 70788 3.23 6.23 0 1008008<br>
xmalloc-testN hd    14.884 104576 18.01 1.64 0 34968<br>
xmalloc-testN hm    7.359 49832 9.66 7.23 0 74091<br>
xmalloc-testN hml   33.674 43692 2.28 9.70 0 140076<br>
xmalloc-testN iso   22.670 36352 6.63 7.87 0 9489<br>
xmalloc-testN je    6.408 54704 17.56 0.91 0 24286<br>
xmalloc-testN lf    3.202 56064 19.15 0.28 0 13736<br>
xmalloc-testN lp    3.815 51348 17.43 0.81 0 32405<br>
xmalloc-testN lt    1.400 30976 17.83 0.79 0 7219<br>
xmalloc-testN mi    1.049 49660 17.31 1.32 0 278490<br>
xmalloc-testN mi-sec 2.128 51068 16.73 1.84 0 299642<br>
xmalloc-testN mi2   0.999 57876 17.91 0.74 0 22005<br>
xmalloc-testN mi2-sec 2.173 56576 18.60 0.41 0 15547<br>
xmalloc-testN mng   13.770 8924 8.25 8.66 0 91634<br>
xmalloc-testN mesh  2.718 34092 12.29 4.89 0 16704<br>
xmalloc-testN rp    0.836 73344 18.39 0.58 0 18150<br>
xmalloc-testN scudo 19.279 52720 16.15 2.07 0 17505<br>
xmalloc-testN sg    3.862 53804 17.68 0.73 0 36154<br>
xmalloc-testN sm    1.657 42368 18.18 0.70 0 13305<br>
xmalloc-testN sn    0.714 49408 17.20 1.14 0 12048<br>
xmalloc-testN sn-sec 1.395 50688 16.35 1.70 0 12349<br>
xmalloc-testN tbb   1.613 53260 18.72 0.48 0 12906<br>
xmalloc-testN tc    12.277 39680 11.09 6.61 0 9098<br>
xmalloc-testN yal   2.235 221012 19.13 0.31 0 63712<br>
xmalloc-testN wbht  4.021 47872 18.91 0.66 0 11700<br>
cache-scratch1 sys   01.21 3712 1.21 0.00 0 212<br>
cache-scratch1 dh    01.19 5632 1.19 0.00 0 286<br>
cache-scratch1 ff    01.20 77952 1.18 0.02 0 18733<br>
cache-scratch1 hd    01.17 4096 1.17 0.00 0 305<br>
cache-scratch1 hm    01.17 4736 1.17 0.00 0 458<br>
cache-scratch1 hml   01.19 3968 1.18 0.00 0 270<br>
cache-scratch1 iso   01.19 28416 1.18 0.01 0 6400<br>
cache-scratch1 je    01.17 4480 1.16 0.00 0 275<br>
cache-scratch1 lf    01.16 5120 1.16 0.00 0 549<br>
cache-scratch1 lp    01.17 4096 1.17 0.00 0 238<br>
cache-scratch1 lt    01.17 3840 1.17 0.00 0 237<br>
cache-scratch1 mi    01.18 3968 1.18 0.00 0 233<br>
cache-scratch1 mi-sec 01.17 4224 1.17 0.00 0 239<br>
cache-scratch1 mi2   01.18 4096 1.17 0.00 0 240<br>
cache-scratch1 mi2-sec 01.17 4096 1.16 0.00 0 248<br>
cache-scratch1 mng   01.15 3712 1.15 0.00 0 219<br>
cache-scratch1 mesh  01.15 4224 1.15 0.00 0 259<br>
cache-scratch1 rp    01.17 4864 1.17 0.00 0 562<br>
cache-scratch1 scudo 01.16 3968 1.16 0.00 0 236<br>
cache-scratch1 sg    01.16 3712 1.16 0.00 0 216<br>
cache-scratch1 sm    01.15 4224 1.14 0.00 0 310<br>
cache-scratch1 sn    01.22 3968 1.22 0.00 0 242<br>
cache-scratch1 sn-sec 01.27 3968 1.26 0.00 0 243<br>
cache-scratch1 tbb   01.30 4096 1.25 0.02 0 283<br>
cache-scratch1 tc    01.30 7808 1.27 0.01 0 1161<br>
cache-scratch1 yal   01.19 3968 1.19 0.00 0 240<br>
cache-scratch1 wbht  01.17 3840 1.17 0.00 0 219<br>
cache-scratchN sys   00.31 3712 1.21 0.00 0 220<br>
cache-scratchN dh    00.31 5888 1.21 0.00 0 301<br>
cache-scratchN ff    00.33 77824 1.22 0.02 0 18740<br>
cache-scratchN hd    00.31 4224 1.21 0.00 0 310<br>
cache-scratchN hm    00.31 4736 1.21 0.00 0 477<br>
cache-scratchN hml   00.32 3968 1.23 0.00 0 277<br>
cache-scratchN iso   00.31 28544 1.21 0.00 0 6425<br>
cache-scratchN je    00.31 4480 1.20 0.00 0 285<br>
cache-scratchN lf    00.31 5120 1.21 0.00 0 558<br>
cache-scratchN lp    00.31 4224 1.21 0.00 0 246<br>
cache-scratchN lt    00.32 3968 1.22 0.00 0 252<br>
cache-scratchN mi    00.31 4224 1.21 0.00 0 240<br>
cache-scratchN mi-sec 00.31 4096 1.22 0.00 0 248<br>
cache-scratchN mi2   00.31 4096 1.21 0.00 0 246<br>
cache-scratchN mi2-sec 00.36 4224 1.38 0.00 0 254<br>
cache-scratchN mng   00.35 3712 1.37 0.00 0 226<br>
cache-scratchN mesh  00.36 4224 1.37 0.00 0 264<br>
cache-scratchN rp    00.32 4864 1.22 0.00 0 569<br>
cache-scratchN scudo 00.31 3968 1.20 0.00 0 254<br>
cache-scratchN sg    00.30 3840 1.19 0.00 0 224<br>
cache-scratchN sm    00.35 4352 1.37 0.00 0 361<br>
cache-scratchN sn    00.35 4096 1.37 0.00 0 263<br>
cache-scratchN sn-sec 00.35 3968 1.34 0.00 0 267<br>
cache-scratchN tbb   00.31 4224 1.20 0.00 0 291<br>
cache-scratchN tc    00.31 7680 1.21 0.00 0 1169<br>
cache-scratchN yal   00.33 3968 1.26 0.01 0 246<br>
cache-scratchN wbht  00.35 3712 1.37 0.00 0 224<br>
glibc-simple sys   03.43 1792 3.42 0.00 0 207<br>
glibc-simple dh    11.07 6144 11.05 0.00 0 374<br>
glibc-simple ff    04.84 39440 3.56 1.25 0 883996<br>
glibc-simple hd    02.17 4480 2.17 0.00 0 396<br>
glibc-simple hm    07.32 7680 7.31 0.01 0 1296<br>
glibc-simple hml   04.46 4608 4.45 0.00 0 405<br>
glibc-simple iso   07.22 35224 7.10 0.11 0 10965<br>
glibc-simple je    01.86 4736 1.85 0.00 0 389<br>
glibc-simple lf    03.06 2944 3.06 0.00 0 572<br>
glibc-simple lp    03.36 1920 3.36 0.00 0 232<br>
glibc-simple lt    01.58 4096 1.58 0.00 0 310<br>
glibc-simple mi    01.41 1920 1.40 0.00 0 255<br>
glibc-simple mi-sec 03.05 2048 3.04 0.00 0 282<br>
glibc-simple mi2   01.39 2176 1.39 0.00 0 291<br>
glibc-simple mi2-sec 03.41 2176 3.41 0.00 0 305<br>
glibc-simple mng   09.64 1792 5.65 3.88 0 1196774<br>
glibc-simple mesh  02.65 2432 2.65 0.00 0 867<br>
glibc-simple rp    01.81 2944 1.80 0.00 0 616<br>
glibc-simple scudo 04.03 4352 4.03 0.00 0 389<br>
glibc-simple sg    03.20 1792 3.19 0.00 0 211<br>
glibc-simple sm    04.06 4608 4.05 0.00 0 473<br>
glibc-simple sn    01.39 2304 1.39 0.00 0 266<br>
glibc-simple sn-sec 01.72 2304 1.72 0.00 0 289<br>
glibc-simple tbb   02.93 4224 2.92 0.00 0 349<br>
glibc-simple tc    01.68 8064 1.67 0.00 0 1253<br>
glibc-simple yal   05.01 2048 5.00 0.00 0 339<br>
glibc-simple wbht  11.71 1792 4.15 7.37 0 1156429<br>
glibc-thread sys   4.219 2432 7.93 0.00 0 362<br>
glibc-thread dh    280.749 6784 2.25 2.95 0 627<br>
glibc-thread ff    36.516 41136 2.45 2.02 0 305460<br>
glibc-thread hd    3.437 7936 7.89 0.02 0 1291<br>
glibc-thread hm    14.950 56832 7.54 0.31 0 21638<br>
glibc-thread hml   8.809 5504 7.91 0.00 0 681<br>
glibc-thread iso   154.267 63488 2.91 2.64 0 16026<br>
glibc-thread je    3.090 5760 7.91 0.00 0 708<br>
glibc-thread lf    4.800 5248 7.87 0.01 0 1075<br>
glibc-thread lp    4.241 2560 7.91 0.00 0 389<br>
glibc-thread lt    3.156 8448 7.91 0.01 0 1456<br>
glibc-thread mi    2.943 2944 7.92 0.00 0 456<br>
glibc-thread mi-sec 6.690 11264 7.89 0.01 0 2568<br>
glibc-thread mi2   2.916 4096 7.92 0.00 0 745<br>
glibc-thread mi2-sec 6.920 12288 7.92 0.00 0 2826<br>
glibc-thread mng   137.145 2208 3.22 2.91 0 36405<br>
glibc-thread mesh  3.932 3436 7.89 0.00 0 1428<br>
glibc-thread rp    3.170 6784 7.92 0.00 0 1566<br>
glibc-thread scudo 6.962 4992 7.91 0.00 0 521<br>
glibc-thread sg    4.486 2176 7.91 0.00 0 367<br>
glibc-thread sm    7.613 9728 7.87 0.01 0 1734<br>
glibc-thread sn    2.887 5120 7.93 0.00 0 986<br>
glibc-thread sn-sec 3.257 17536 7.88 0.01 0 4094<br>
glibc-thread tbb   5.474 5504 7.92 0.00 0 661<br>
glibc-thread tc    3.325 8832 7.91 0.00 0 1479<br>
glibc-thread yal   7.596 5228 7.85 0.03 0 1970<br>
glibc-thread wbht  7.963 3192 7.89 0.04 0 9407<br>

# Reference
1. R. P. Brent (July 1989). "Efficient Implementation of the First-Fit Strategy for Dynamic Storage Allocation". ACM Transactions on Programming Languages and Systems, Vol. 11, No. 3, July 1989. p. 388.
2. https://github.com/daanx/mimalloc-bench

