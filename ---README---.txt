【一些问题】
1.tacopia解决超过1024的问题：
	参考libevent库中讲到的动态fd_array的方法，能够达到的效果为万级别
	那么select一次监听超过上万套接字性能会如何呢？
	
2.tcp_gateway解决恶意连接(连接后不发数据或不接数据)

3.tcp_gateway一些特殊情况
thread1 		thread2			描述
-----------------------------------------------------
s:reead_over	
				c:close_over
				s:disconnect
				s:close_over	回收连接，释放内存
c:post_write					对象已释放，抛出异常
s:post_read

【注意事项】
调用closesocket()函数socket时请确保当前程序没有残留关于
该socket的任何相关操作请求。
(如果不是这样，关闭后该套接字可能会被重用，当残留的操作请求触发时会得到不想要的结果)
