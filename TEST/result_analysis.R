library(ggplot2)
#VMAF Angel
data_vmaf = as.data.frame(read.table('/home/hpcn/Repos/Packetization-LHE/TEST/DATA/full_results.csv',header=T,sep=","))
data_vmaf$
g1 <- ggplot(data_vmaf,aes(x=Loss_Measured,y=VMAF_Score,col=as.factor(Packet_Size)))
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=as.factor(Loss_Set)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top", legend.text = element_text(size=8), legend.title  = element_text(size=9)  )
g1 <- g1 + labs(x="Perdidas medidas (%)",y="Puntuación VMAF", col="Tamaño\n paquete(bytes)", shape="Pérdidas\n fijadas(%)") + geom_smooth(se = FALSE, size=0.5)+ theme(axis.text = element_text(size=12))+ theme(axis.title= element_text(size=14))
g1
g2 <- ggplot(data_vmaf,aes(x=Loss_Measured,y=VMAF_Score,col=interaction(Block_W,Block_H)))
g2 <- g2 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=as.factor(Loss_Set)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top", legend.text = element_text(size=8), legend.title  = element_text(size=9)  )
g2 <- g2 + labs(x="Perdidas medidas (%)",y="Puntuación VMAF", col="Tamaño\n bloque", shape="Pérdidas\n fijadas(%)") + geom_smooth(se = FALSE, size=0.5)+ theme(axis.text = element_text(size=12))+ theme(axis.title= element_text(size=14))
g2
#VMAF Jesus
data_vmaf = as.data.frame(read.table('/home/hpcn/Repos/Packetization-LHE/TEST/bbdd_15-60.csv',header=T,sep=","))

data_vmaf$Video
#calidad segun perdidas por tamaño de bloque 
g1 <- ggplot(data_vmaf,aes(x=Video_Size/30/1e6, y=VMAF_Score,shape=interaction(Framerate, Resolution),col=interaction(Framerate, Resolution)))
#data=data_vmaf[data_vmaf$Resolution==data_vmaf$Resolution[277],],
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_jitter(aes(size=as.factor(YUV_Profile)),alpha=0.5) + theme_grey(base_size=6) + theme(legend.position="top", legend.text = element_text(size=8), legend.title  = element_text(size=9)  )
g1 <- g1 +geom_smooth(se = FALSE, size=0.5, method="lm")+ theme(axis.text = element_text(size=14))+ theme(axis.title= element_text(size=18))
g1 <- g1 + labs(x="Bitrate (Mb/s)",y="VMAF Score",col="Framerate &\n Resolution", shape= "Framerate &\n Resolution", size="Color\n Profile ")
g1 <- g1 + guides(colour = guide_legend(override.aes = list(shape =interaction(Framerate, Resolution))))
#g1 <- g1 + scale_shape(guide = FALSE)
g1

#data=data_vmaf[data_vmaf$Resolution==data_vmaf$Resolution[600]&data_vmaf$Jitter==1&data_vmaf$YUV_Profile==data_vmaf$YUV_Profile[1],],
g1 <- ggplot(data_vmaf,aes(x=Loss_Measured, y=VMAF_Score,shape=interaction(Framerate, Resolution),col=interaction(Framerate, Resolution)))
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_jitter(aes(size=as.factor(YUV_Profile)),alpha=0.5) + theme_grey(base_size=20) + theme_grey(base_size=6) + theme(legend.position="top", legend.text = element_text(size=8), legend.title  = element_text(size=9)  )
g1 <- g1 + geom_smooth(se = FALSE, size=0.5, method="lm")+ theme(axis.text = element_text(size=14))+ theme(axis.title= element_text(size=18))
g1 <- g1 + labs(x="Measured Loss(%)",y="VMAF Score",col="Framerate &\n Resolution", shape= "Framerate &\n Resolution", size="Color\n Profile ")
g1 <- g1 + guides(colour = guide_legend(override.aes = list(shape =interaction(Framerate, Resolution))))
#g1 <- g1 + scale_shape(guide = FALSE)
g1

g1 <- ggplot(data_vmaf,aes(x=Loss_Measured,y=VMAF_Score,col=interaction(Latency, Bandwidth)))
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_point(data=data_vmaf[data_vmaf$Resolution==data_vmaf$Resolution[600],],aes(shape=as.factor(Resolution)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top", legend.text = element_text(size=10), legend.title  = element_text(size=12))
g1 <- g1 + labs(x="Pérdidas medidas (%)",y="Puntuación VMAF", col="Pérdidas\n fijadas (%)", shape="Ancho de\n banda (Mb/s)")+ theme(axis.text = element_text(size=14))+ theme(axis.title= element_text(size=18)) #+ geom_smooth(aes(group=as.factor(Bandwidth),col=as.factor(Loss_Set)),se = FALSE, size=0.5,method='lm')
g1

g1 <- ggplot(data_vmaf[data_vmaf$Latency==0 & data_vmaf$Bandwidth==100,],aes(x=Loss_Measured,y=VMAF_Score,col=interaction(Resolution)))
#data=data_vmaf[data_vmaf$Resolution==data_vmaf$Resolution[277],],
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_jitter(aes(shape=interaction(Framerate,YUV_Profile)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top", legend.text = element_text(size=10), legend.title  = element_text(size=12)  )
g1 <- g1 + labs(x="Jitter (ms)",y="Pérdidas medidas",col="Pérdidas\n medidas (%)",shape="Resolucion") + geom_smooth(method="lm",se = FALSE, size=0.5)+ theme(axis.text = element_text(size=14))+ theme(axis.title= element_text(size=18))
g1

g1 <- ggplot(data_vmaf,aes(x=Bandwidth,y=VMAF_Score,col=as.factor(Resolution)))
g1 <- g1 + scale_shape_manual(values = 5:25) + geom_jitter(aes(shape=as.factor(Latency)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top")
g1 <- g1 + labs(x="Loss measured (%)",y="VMAF Score") + geom_smooth(aes(group=as.factor(Bandwidth),col=as.factor(Loss_Set)),se = FALSE, size=0.5,method='lm')
g1

#calidad segun ratio perimetro-area por perdidas y alto de bloque
g2 <- ggplot(data_vmaf,aes(x=PtoA_R,y=VMAF_Score,col=as.factor(Loss_Set)))
g2 <- g2 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=as.factor(Block_H)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top")
g2 <- g2 + labs(x="PtoA",y="VMAF_Score") + geom_smooth(se = FALSE, size=0.5)

summary((lm(data=data_vmaf,VMAF_Score~PtoA_R*Loss_Set)))

#ancho segun tamaño de paquete por tamaño de bloque
g3 <- ggplot(data_vmaf,aes(x=Packet_Size,y=MbS,col=interaction(Block_W,Block_H)))
g3 <- g3 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=interaction(Block_W,Block_H)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top")
g3 <- g3 + labs(x="Packet_Size",y="MbS") + geom_smooth(se = FALSE, size=0.5)

#ancho segun tamaño de paquete por tamaño de bloque y cantidad de pixeles
g4 <- ggplot(data_vmaf,aes(x=Packet_Size,y=MbS,col=as.factor(Pix)))
g4 <- g4 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=interaction(Block_W,Block_H)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top")
g4 <- g4 + labs(x="Packet_Size",y="MbS") + geom_smooth(se = FALSE, size=0.5)

#calidad frente a perdidas segun tamaño de paquete
g5 <- ggplot(data_vmaf,aes(x=Loss_Measured,y=VMAF_Score,col=as.factor(Packet_Size)))
g5 <- g5 + scale_shape_manual(values = 5:25) + geom_point(aes(shape=as.factor(Packet_Size)),size=4,alpha=0.7) + theme_grey(base_size=20) + theme(legend.position="top")
g5 <- g5 + labs(x="Loss_Measured",y="VMAF_Score") + geom_smooth(se = FALSE, size=0.5)

summary((lm(data=data_vmaf,VMAF_Score~Loss_Set)))


library(plotly)


plot_ly(x=data_vmaf$Loss_Measured, y=data_vmaf$Video_Size/30, z=data_vmaf$VMAF_Score, type="scatter3d", mode="markers", color=data_vmaf$Loss_Measured,split=interaction(data_vmaf$Framerate, data_vmaf$Resolution))

x11()
plot(g1)
x11()
plot(g2)
x11()
plot(g3)
x11()
plot(g4)
x11()
plot(g5)

ggsave("/home/hpcn/Repos/Packetization-LHE/TEST/DATA/test_LSM_VMAF_Lt_BW.png",g1,paper = "a4r",width=12, height=8, scale=1)
ggsave("/home/hpcn/Repos/Packetization-LHE/TEST/DATA/test_VMAF2.pdf",g2,paper = "a4r",width=12, height=8, scale=1)
ggsave("/home/hpcn/Repos/Packetization-LHE/TEST/DATA/test_VMAF3.pdf",g3,paper = "a4r",width=12, height=8, scale=1)
