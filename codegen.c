#include "9cc.h"


int IfEndLNum; // condegenで初期化
int IfElseLNum; // condegenで初期化
int WhileBeginLNum; // condegenで初期化
int WhileEndLNum; // condegenで初期化
int ForBeginLNum;
int ForEndLNum;
int StackAlign;

// ローカス変数の場合
// Nodeが変数の場合、そのアドレスを計算して、それをスタックへプッシュ
// プロローグでRBPの値(メモリアドレス)を設定しているので、
// そのアドレスからのオフセット分を計算した値(メモリアドレス)をRAXへ返し、
// それをスタックへプッシュする
void gen_lvar(Node *node){
	if(node->kind != ND_LVAR)
		error("代入の左辺値が変数ではありません。");

	printf("	mov rax, rbp\n"); // RBPの値(レジスタアドレス)をRAXレジスタへ代入
	printf("	sub rax, %d\n", node->offset); // RAXの値からオフセット値を引き、その結果をRAXへ
	printf("	push rax\n"); // RAXをスタックトップへプッシュ
}


// 抽象構文木を下りながらコードを生成する
void gen(Node *node){
	
	if(node->kind == ND_RETURN){
		gen(node->rhs); // exprが試行された結果がRaxへpushされる
		printf("	pop rax\n");
		printf("	mov rsp, rbp\n");
		printf("	pop rbp\n");
		printf("	ret\n");
		return;
	}
	else if(node->kind == ND_NUM){ //木終端の数字である場合、それをスタックへプッシュ
		printf("	push %d\n", node->val);
		return;
	}
	else if(node->kind == ND_LVAR){ //木終端が変数である場合、変数の値をスタックへ積む
		gen_lvar(node); // 変数のメモリアドレスを計算して、スタックトップへ積む(RSPが示す番地の値)
		printf("	pop rax\n"); // gen_lvarで計算したRSPの内容(メモリアドレス)をRAXへ代入
		printf("	mov rax, [rax]\n"); // RAXが示すメモリアドレスの値(変数の値)をRAXへ代入
		printf("	push rax\n"); // RAXの値をスタックトップへ積む
		return;
	}
	else if(node->kind == ND_ASSIGN){ // 宣言である場合。a=3などの処理を行う。
		gen_lvar(node->lhs); // 左側ノードは変数であるはずなので、そのメモリアドレスを計算してスタックトップへ積む
		gen(node->rhs); // 右側ノードのコーディング。数字、アドレスのプッシュ / 宣言の処理

		printf("	pop rdi\n"); // 宣言の場合、右側ノードは数値であるので、RSIは数字
		printf("	pop rax\n"); // 宣言の場合、左側ノードは変数であるので、RAXはメモリアドレスになる
		printf("	mov [rax], rdi\n"); // 変数アドレスへRDI値を代入
		printf("	push rdi\n"); // RDI値をスタックトップへ積む
		return;
	}
	else if(node->kind == ND_IF){ // if ノードである場合
		gen(node->lhs); // 左側ノードのコーディング 論理演算の結果がRAXへpushされる

		printf("	pop rax\n"); 
		printf("	cmp rax, 0\n"); // 結果が0(false)であることを比較
		printf("	je	.LIfEnd%04d\n", IfEndLNum);

		gen(node->rhs); // 右側ノードのコーディング ifのstmt
		printf(".LIfEnd%04d:\n", IfEndLNum++);
		return;

	}
	else if(node->kind == ND_IFEL){ // if-else ノード
		gen(node->lhs); // 左側ノードをコーディング。ifの条件式の結果がRAXへpushされる

		printf("	pop rax\n");
		printf("	cmp rax, 0\n");
		printf("	je	.LElse%04d\n",  IfElseLNum);
		gen(node->rhs); // 右側ノードをコーディング。右側はelseノードになっているので、次の再帰呼出しで処理
		return;
	}
	else if(node->kind == ND_ELSE){ // else ノード
		gen(node->lhs); // if の stmtのコード

		printf("	jmp .LIfEnd%04d\n",  IfEndLNum );
		printf(".LElse%04d:\n",IfElseLNum++);
		gen(node->rhs); // else の stmtのコード
		printf(".LIfEnd%04d:\n", IfEndLNum++);
		return;
	}
	else if(node->kind == ND_WHILE){
		printf(".LWhileBegin%04d:\n", WhileBeginLNum);
		gen(node->lhs); // 条件式のコーディング
		
		printf("	pop rax\n");
		printf("	cmp rax, 0\n");
		printf("	je .LWhileEnd%04d\n", WhileEndLNum);
		gen(node->rhs);
		printf("	jmp .LWhileBegin%04d\n", WhileBeginLNum);
		printf(".LWhileEnd%04d:", WhileEndLNum++);
		return;
	}
	else if(node->kind == ND_FOR){ // for(A; B; C) D
		gen(node->lhs); // Aのコーディング
		printf(".LForBegin%04d:\n", ForBeginLNum);
		gen(node->rhs); // ND_FORCOND1ノード
		return;
	}
	else if(node->kind == ND_FORCOND1){ // for(A; B; C) D
		gen(node->lhs); // Bのコーディング
		printf("	pop rax\n");
		printf("	cmp rax, 0\n");
		printf("	je .LForEnd%04d\n", ForEndLNum);
		gen(node->rhs); // ND_FORCOND2ノード
		return;
	}
	else if(node->kind == ND_FORCOND2){ // for(A; B; C) D
		gen(node->rhs); // Dのコーディング
		gen(node->lhs); // Cのコーディング
		printf("	jmp .LForBegin%04d\n", ForBeginLNum);
		printf(".LForEnd%04d:\n", ForEndLNum++);
		return;
	}
	else if(node->kind == ND_BLOCK){ // "{" stmt* "}"
		// stmt* はNodeリンクとして設定している
		// 次のリンクがNULLになるまで stmt のコーディングを行う
		for(Node *cur = node->stmtLink; cur; cur = cur->stmtLink){
			gen(cur); // stmtのコーディング
		}
		return;
	}
	else if(node->kind == ND_FUNCTION_CALL){
		for(Node *cur = node->parameterLink; cur; cur = cur->parameterLink){
			gen(cur); // 第一引数の結果から順番にpushしていく
		}

		char *registers[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
		for(int i = node->parameterNumber - 1; i >= 0; i--){
			printf("	pop %s\n", registers[i]);
		}

		char dst[100];
		memset(dst, '\0', sizeof(dst));
		strncpy(dst, node->f_name, node->f_name_len);
		// RSPの値が16バイト境界かを調べる
		printf("	mov rax, rsp\n"); // RSPの値をRAXへ
		printf("	and rax, 0b1111\n"); // 15(0b1111)と&を取る
		printf("	jnz .LStackAlign%d\n", StackAlign); // jnz 結果が0出ない場合
		printf("	call %s\n", dst);
		printf("	jmp .LStackAlignEnd%d\n", StackAlign);
		printf(".LStackAlign%d:\n", StackAlign);
		printf("	sub rsp, 8\n"); // RSPから8バイトを引いて16バイト境界へする(POP, PUSHは8バイト単位でRSPを操作するので,RSPが16バイト境界に無い場合は8バイト境界になっている)
		printf("	call %s\n", dst);
		printf("	add rsp, 8\n"); // 差し引いた8バイトを元に戻す
		printf(".LStackAlignEnd%d:\n", StackAlign++);
		printf("	push rax\n"); // 関数戻り値をスタックへ積む
		return;
	
	}
	else if(node->kind == ND_NULL){
		return;
	}

	gen(node->lhs); // 左のノードをコーディング。数字のプッシュ。変数アドレスのプッシュ、宣言の処理
	gen(node->rhs); // 右のノードをコーディング。

	printf("	pop rdi\n"); //後にスタックした値を先に取り出す。右側のノードの値
	printf("	pop rax\n"); //先のスタックした値を後に取り出す。左側のノードの値

	switch(node->kind){ // ノードの種類によりrax, rdiレジスタを操作する
		case ND_ADD:
			printf("	add rax, rdi\n");
			break;
		case ND_SUB:
			printf("	sub rax, rdi\n");
			break;
		case ND_MUL:
			printf("	imul rax, rdi\n");
			break;
		case ND_DIV:
			printf("	cqo\n");
			printf("	idiv rdi\n");
			break;
		case ND_EQ:
			printf("	cmp rax, rdi\n"); // cmpの操作
			printf("	sete al\n"); // == の結果をフラグレジスタからalレジスタへ
			printf("	movzb rax, al\n"); // alは8bitなので、64bitへ展開した結果をraxへセットする。このとき上位56bitはゼロで埋める
			break;
		case ND_NE:
			printf("	cmp rax, rdi\n");
			printf("	setne al\n"); // cmpの!= の結果をフラグレジスタからalレジスタへ
			printf("	movzb rax, al\n");
			break;
		case ND_LT:
			printf("	cmp rax, rdi\n");
			printf("	setl al\n"); // cmpの< の結果をフラグレジスタからalレジスタへ
			printf("	movzb rax, al\n");
			break;
		case ND_LE:
			printf("	cmp rax, rdi\n");
			printf("	setle al\n"); // cmpの <= の結果をフラグレジスタからalレジスタへ
			printf("	movzb rax, al\n");
			break;
	}

	printf("	push rax\n"); // 演算結果が格納されているraxレジスタをスタックへプッシュする
}

void codegen(){
	
	//LabelNumberの初期化
	IfEndLNum = 1; 
	IfElseLNum = 1;
	WhileBeginLNum = 1;
	WhileEndLNum = 1;
	ForBeginLNum = 1;
	ForEndLNum = 1;
	StackAlign = 1;

	// アセンブリの前半部分を出力
	printf(".intel_syntax noprefix\n");
	printf(".global main");
	for(FName *fname = functionNames; fname; fname = fname->next){
		if(fname->name != NULL){
		//	fprintf(stderr, "%s, %d\n", fname->name, fname->len);
			char d[20];
			memset(d, '\0', sizeof(d));
			strncpy(d, fname->name, fname->len);
			printf(", %s", d);
		}
	}
	printf("\n");
	printf("main:\n");

	// プロローグ
	printf("	push rbp\n"); // RBPレジスタ（ベースレジスタ)の値(ベースポインタ値)を
	// RSPレジスタが指すスタックの先頭へプッシュする（メモリアクセス)
	printf("	mov rbp, rsp\n"); // RSPレジスタ値をRBPレジスタへ代入(メモリアドレス RBP = RSP)
	printf("	sub rsp, %d\n", locals->offset); // ローカル変数のオフセット分スタックを確保する 
	// 呼び出し時点のRBP	RBP
	// 
	// 
	// locals->next>offset
	// locals									RSP

	for(int i = 0; code[i]; i++){ // Codeの末尾はNULL
		gen(code[i]);
		
		// 式の評価結果としてスタックに１つの値が残っているはずなので、
		// スタックが溢れないようにポップしておく
		printf("	pop rax\n");
	}

	// スタックトップに式全体の値が残っているはずなので
	// それをRAXにロードして関数からの返り値とする
	// エピローグ
	printf("	mov rsp, rbp\n");
	printf("	pop rbp\n");
	printf("	ret\n");	
}


