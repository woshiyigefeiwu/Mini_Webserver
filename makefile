# 定义变量
src = http_conn.o log.o lst_timer.o main.o
target = app

# 规则1
$(target):$(src)
	g++ $(src) -o $(target)

# 规则2
%.o : %.c       # 进行模式匹配
	g++ -c $< -pthread -o $@
	
.PHONY: clean
clean:
	rm -f *.o
