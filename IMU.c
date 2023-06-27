/******************** (C) COPYRIGHT 2015 DUT ********************************
 * ����    �����Ĳ�
 * �ļ���  ��IMU.c
 * ����    ����̬�����㷨
 * ����    ��2015/11/30 12:43:38
 * ��ϵ��ʽ��1461318172��qq��
**********************************************************************************/
/********************
  * @attention
  *
  *ռ��STM32 ��Դ��
  *1. ʹ��Tim7��ʱ�� ����us����ϵͳʱ��
  ******************************************************************************
 */

#include "IMU.h"


#define DATA_SIZE 100
volatile float exInt, eyInt, ezInt;  // ������
volatile float integralFBx, integralFBy, integralFBz;
volatile float q0, q1, q2, q3; // ȫ����Ԫ��
volatile float qa0, qa1, qa2, qa3;
volatile float integralFBhand, handdiff;
volatile double halftime ;
volatile uint32_t lastUpdate, now; // �������ڼ��� ��λ us
volatile uint16_t sysytem_time_ms = 0;
volatile float IMU_Pitch, IMU_Roll, IMU_Yaw, ACC_Pitch, ACC_Roll;// ��̬�Ƕ� ��λ ��
volatile float  IMU_GYROx, IMU_GYROy, IMU_GYROz;//��̬������ ��λ ��ÿ��
volatile unsigned char IMU_inited = 0;
volatile uint16_t imu_clce = 0;
volatile float acc_vector = 0;  //��ǰ���ٶȸ�Ӧ��������  M/S^2
volatile float  acc_X, acc_Y, acc_Z, acc_MX, acc_MY, acc_MZ; //���ٶȳ�ʼ����Ԫ��
// Fast inverse square-root
/**************************ʵ�ֺ���********************************************
*����ԭ��:	   float invSqrt(float x)
*��������:	   ���ټ��� 1/Sqrt(x)
��������� Ҫ�����ֵ
��������� ���
*******************************************************************************/
float invSqrt(float x)
{
	volatile float halfx = 0.5f * x;
	volatile float y = x;
	long i = *(long *)&y;
	i = 0x5f3759df - (i >> 1);
	y = *(float *)&i;
	y = y * (1.5f - (halfx * y * y));
	return y;
}

/**************************ʵ�ֺ���********************************************
*����ԭ��:	   void IMU_init(void)
*��������:	  ��ʼ��IMU���
			  ��ʼ������������
			  ��ʼ����Ԫ��
			  ����������
			  ����ϵͳʱ��
�����������
���������û��
*******************************************************************************/
void IMU_init(void)
{
	MPU6050_initialize();
	HMC5883L_SetUp();
	delay_ms(50);
	MPU6050_initialize();
	HMC5883L_SetUp();

	// initialize quaternion
	q0 = 1.0f;  //��ʼ����Ԫ��
	q1 = 0.0f;
	q2 = 0.0f;
	q3 = 0.0f;
	qa0 = 1.0f;  //��ʼ����Ԫ��
	qa1 = 0.0f;
	qa2 = 0.0f;
	qa3 = 0.0f;
	exInt = 0.0;
	eyInt = 0.0;
	ezInt = 0.0;
	integralFBx = 0.0;
	integralFBy = 0.0;
	integralFBz	= 0.0;
	lastUpdate = micros();//����ʱ��
	now = micros();
}

/**************************ʵ�ֺ���********************************************
*����ԭ��:	void IMU_getValues(volatile float * values)
*��������:	��ȡ���ٶ� ������ ������ �ĵ�ǰֵ
��������� 	�������ŵ������׵�ַ
���������û��
*******************************************************************************/
#define new_weight 0.4f//��Ȩ��
#define old_weight 0.6f//��Ȩ��

void IMU_getValues(float *values)
{
	int16_t accgyroval[6];
	static  volatile float lastacc[3] = {0, 0, 0};
	int i;
	//��ȡ���ٶȺ������ǵĵ�ǰADC
	MPU6050_getMotion6(&accgyroval[0], &accgyroval[1], &accgyroval[2], &accgyroval[3], &accgyroval[4], &accgyroval[5]);
	for(i = 0; i < 6; i++)
	{
		if(i < 3)
		{
			values[i] = (float) accgyroval[i] * new_weight + lastacc[i] * old_weight ;
			lastacc[i] = values[i];
		}
		else
		{
			values[i] = ((float) accgyroval[i]) / 16.4f; //ת�ɶ�ÿ��
			//�����Ѿ������̸ĳ��� 2000��ÿ��  16.4 ��Ӧ 1��ÿ��
		}
	}
	HMC58X3_mgetValues(&values[6]);	//��ȡ�����Ƶ�ADCֵ
	IMU_GYROx = values[3];
	IMU_GYROy = values[4];
	IMU_GYROz = values[5];

}


/**************************ʵ�ֺ���********************************************
*����ԭ��:	   void IMU_AHRSupdate
*��������:	 ����AHRS ������Ԫ��
��������� ��ǰ�Ĳ���ֵ��
���������û��
*******************************************************************************/
#define Kp 2.0f   // proportional gain governs rate of convergence to accelerometer/magnetometer
#define Ki 0.03f   // integral gain governs rate of convergence of gyroscope biases

void IMU_AHRSupdate(volatile float gx, volatile float gy, volatile float gz, volatile float ax, volatile float ay, volatile float az, volatile float mx, volatile float my, volatile float mz)
{

	volatile  float norm;
	volatile float hx, hy, hz, bx, bz;
	volatile float vx, vy, vz, wx, wy, wz;
	volatile float ex, ey, ez, halfT;
	volatile float temp0, temp1, temp2, temp3;
	volatile float temp;
	// �Ȱ���Щ�õõ���ֵ���
	float q0q0 = q0 * q0;
	float q0q1 = q0 * q1;
	float q0q2 = q0 * q2;
	float q0q3 = q0 * q3;
	float q1q1 = q1 * q1;
	float q1q2 = q1 * q2;
	float q1q3 = q1 * q3;
	float q2q2 = q2 * q2;
	float q2q3 = q2 * q3;
	float q3q3 = q3 * q3;

	now = micros();  //��ȡʱ��
	if(now < lastUpdate)  //��ʱ��������ˡ�
	{
		halfT =  ((float)(now + (0xffffffff - lastUpdate)) / 2000000.0f);
		lastUpdate = now;
		//return ;
	}
	else
	{
		halfT =  ((float)(now - lastUpdate) / 2000000.0f);
	}
	halftime = halfT;
	lastUpdate = now;	//����ʱ��

	temp = sqrt(ax * ax + ay * ay + az * az);
	temp = (temp / 16384.0f) * 9.8f;   //ת��M/S^2Ϊ��λ��
	acc_vector = acc_vector +   //��ͨ�˲�����ֹƵ��20hz
		     (halfT * 2.0f / (7.9577e-3f + halfT * 2.0f)) * (temp - acc_vector);

	norm = invSqrt(ax * ax + ay * ay + az * az);
	ax = ax * norm;
	ay = ay * norm;
	az = az * norm;
	//�ü��ٶȼ���roll��pitch
	//	temp = ax * invSqrt((ay * ay + az * az));
	//	ACC_Pitch = atan(temp)* 57.3;
	//
	//	temp = ay * invSqrt((ax * ax + az * az));
	//	ACC_Roll = atan(temp)* 57.3;

	norm = invSqrt(mx * mx + my * my + mz * mz);
	mx = mx * norm;
	my = my * norm;
	mz = mz * norm;

	// compute reference direction of flux
	//�ӻ�������ϵ�ĵ������̲⵽��ʸ��ת�ɵ�������ϵ�µĴų�ʸ��hxyz������ֵ��
	hx = 2 * mx * (0.5f - q2q2 - q3q3) + 2 * my * (q1q2 - q0q3) + 2 * mz * (q1q3 + q0q2);
	hy = 2 * mx * (q1q2 + q0q3) + 2 * my * (0.5f - q1q1 - q3q3) + 2 * mz * (q2q3 - q0q1);
	hz = 2 * mx * (q1q3 - q0q2) + 2 * my * (q2q3 + q0q1) + 2 * mz * (0.5f - q1q1 - q2q2);
	/*
	�����������ϵ�µĴų�ʸ��bxyz���ο�ֵ����
	��Ϊ����ش�ˮƽ�нǣ�������֪��0�ȣ���ȥ��ƫ�ǵ����أ��̶��򱱣�������by=0��bx=ĳֵ
	������ο��ش�ʸ���ڴ�ֱ����Ҳ�з���bz��������ÿ���ط����ǲ�һ���ġ�
	�����޷���֪��Ҳ���޷������ںϣ��и��ʺ�����ֱ���������ںϵļ��ٶȼƣ�������ֱ�ӴӲ���ֵhz�ϸ��ƹ�����bz=hz��
	�ų�ˮƽ�������ο�ֵ�Ͳ���ֵ�Ĵ�СӦ����һ�µ�(bx*bx) + (by*by)) = ((hx*hx) + (hy*hy))��
	��Ϊby=0�����Ծͼ򻯳�(bx*bx)  = ((hx*hx) + (hy*hy))�������bx��
	*/
	bx = sqrt((hx * hx) + (hy * hy));
	bz = hz;

	// estimated direction of gravity and flux (v and w)
	/*
	���ǰ���Ԫ������ɡ��������Ҿ����еĵ����е�����Ԫ�ء�
	�������Ҿ����ŷ���ǵĶ��壬��������ϵ������������ת����������ϵ��������������Ԫ�ء�
	���������vx\y\z����ʵ���ǵ�ǰ��ŷ���ǣ�����Ԫ�����Ļ����������ϵ�ϣ����������������λ������
	*/
	vx = 2 * (q1q3 - q0q2);
	vy = 2 * (q0q1 + q2q3);
	vz = q0q0 - q1q1 - q2q2 + q3q3;
	/*
	���ǰѵ�������ϵ�ϵĴų�ʸ��bxyz��ת����������wxyz��
	��Ϊby=0�����������漰��by�Ĳ��ֶ���ʡ���ˡ�
	������������vxyz�����㣬��Ϊ����g��gz=1��gx=gy=0�����������漰��gxgy�Ĳ���Ҳ��ʡ����
	*/
	wx = 2 * bx * (0.5f - q2q2 - q3q3) + 2 * bz * (q1q3 - q0q2);
	wy = 2 * bx * (q1q2 - q0q3) + 2 * bz * (q0q1 + q2q3);
	wz = 2 * bx * (q0q2 + q1q3) + 2 * bz * (0.5f - q1q1 - q2q2);

	// error is sum of cross product between reference direction of fields and direction measured by sensors
	//���ڰѼ��ٶȵĲ���ʸ���Ͳο�ʸ����������Ѵų��Ĳ���ʸ���Ͳο�ʸ��Ҳ����������������������ݡ�
	ex = (ay * vz - az * vy) + (my * wz - mz * wy);
	ey = (az * vx - ax * vz) + (mz * wx - mx * wz);
	ez = (ax * vy - ay * vx) + (mx * wy - my * wx);
	/*
	axyz�ǻ����������ϵ�ϣ����ٶȼƲ����������������Ҳ����ʵ�ʲ����������������
	axyz�ǲ����õ�������������vxyz�����ݻ��ֺ����̬����������������������Ƕ��ǻ����������ϵ�ϵ�����������
	������֮�������������������ݻ��ֺ����̬�ͼӼƲ��������̬֮�����
	������������������������Ҳ�������������ˣ�����ʾ��exyz�����������������Ĳ����
	�����������Ծ���λ�ڻ�������ϵ�ϵģ������ݻ������Ҳ���ڻ�������ϵ�����Ҳ���Ĵ�С�����ݻ����������ȣ����������������ݡ���������Լ��ö�������һ�£����������ǶԻ���ֱ�ӻ��֣����Զ����ݵľ�������ֱ�������ڶԻ�������ϵ�ľ�����
	*/
	if(ex != 0.0f && ey != 0.0f && ez != 0.0f)
	{
		exInt = exInt + ex * Ki * halfT;
		eyInt = eyInt + ey * Ki * halfT;
		ezInt = ezInt + ez * Ki * halfT;

		// adjusted gyroscope measurements
		// �ò���������PI����������ƫ
		gx = gx + (Kp * ex + exInt);
		gy = gy + (Kp * ey + eyInt);
		gz = gz + (Kp * ez + ezInt);

	}

	// integrate quaternion rate and normalise
	// ��Ԫ��΢�ַ���
	temp0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * halfT;
	temp1 = q1 + (q0 * gx + q2 * gz - q3 * gy) * halfT;
	temp2 = q2 + (q0 * gy - q1 * gz + q3 * gx) * halfT;
	temp3 = q3 + (q0 * gz + q1 * gy - q2 * gx) * halfT;

	// normalise quaternion
	norm = invSqrt(temp0 * temp0 + temp1 * temp1 + temp2 * temp2 + temp3 * temp3);
	q0 = temp0 * norm;
	q1 = temp1 * norm;
	q2 = temp2 * norm;
	q3 = temp3 * norm;
}


#define twoKpDef  (1.0f ) // 2 * proportional gain
#define twoKiDef  (0.2f) // 2 * integral gain

void FreeIMU_AHRSupdate(volatile float gx, volatile float gy, volatile float gz, volatile float ax, volatile float ay, volatile float az)
{
	volatile float norm;
	//  float hx, hy, hz, bx, bz;
	volatile float vx, vy, vz;
	volatile float ex, ey, ez;
	volatile float temp0, temp1, temp2, temp3;

	// �Ȱ���Щ�õõ���ֵ���
	volatile float q0q0 = qa0 * qa0;
	volatile float q0q1 = qa0 * qa1;
	volatile float q0q2 = qa0 * qa2;
	volatile float q0q3 = qa0 * qa3;
	volatile float q1q1 = qa1 * qa1;
	volatile float q1q2 = qa1 * qa2;
	volatile float q1q3 = qa1 * qa3;
	volatile float q2q2 = qa2 * qa2;
	volatile float q2q3 = qa2 * qa3;
	volatile float q3q3 = qa3 * qa3;

	norm = invSqrt(ax * ax + ay * ay + az * az);
	ax = ax * norm;
	ay = ay * norm;
	az = az * norm;

	// estimated direction of gravity and flux (v and w)
	vx = 2 * (q1q3 - q0q2);
	vy = 2 * (q0q1 + q2q3);
	vz = q0q0 - q1q1 - q2q2 + q3q3;

	// error is sum of cross product between reference direction of fields and direction measured by sensors
	ex = (ay * vz - az * vy) ;
	ey = (az * vx - ax * vz) ;
	ez = (ax * vy - ay * vx) ;

	if(ex != 0.0f && ey != 0.0f && ez != 0.0f)
	{

		integralFBx +=  ex * twoKiDef * halftime;
		integralFBy +=  ey * twoKiDef * halftime;
		integralFBz +=  ez * twoKiDef * halftime;

		gx = gx + twoKpDef * ex + integralFBx;
		gy = gy + twoKpDef * ey + integralFBy;
		gz = gz + twoKpDef * ez + integralFBz;

	}
	// integrate quaternion rate and normalise
	temp0 = qa0 + (double)(-qa1 * gx - qa2 * gy - qa3 * gz) * halftime;
	temp1 = qa1 + (double)(qa0 * gx + qa2 * gz - qa3 * gy) * halftime;
	temp2 = qa2 + (double)(qa0 * gy - qa1 * gz + qa3 * gx) * halftime;
	temp3 = qa3 + (double)(qa0 * gz + qa1 * gy - qa2 * gx) * halftime;

	// normalise quaternion
	norm = invSqrt(temp0 * temp0 + temp1 * temp1 + temp2 * temp2 + temp3 * temp3);
	qa0 = temp0 * norm;
	qa1 = temp1 * norm;
	qa2 = temp2 * norm;
	qa3 = temp3 * norm;
}

/**************************ʵ�ֺ���********************************************
*����ԭ��:	   void IMU_getQ(float * q)
*��������:	 ������Ԫ�� ���ص�ǰ����Ԫ����ֵ
��������� ��Ҫ�����Ԫ���������׵�ַ
���������û��
*******************************************************************************/
float mygetqval[9];	//���ڴ�Ŵ�����ת�����������
void IMU_getQ(float *q)
{

	IMU_getValues(mygetqval);
	//�������ǵĲ���ֵת�ɻ���ÿ��
	//���ٶȺʹ����Ʊ��� ADCֵ������Ҫת��
	IMU_AHRSupdate(mygetqval[3] * M_PI / 180, mygetqval[4] * M_PI / 180, mygetqval[5] * M_PI / 180,
		       mygetqval[0], mygetqval[1], mygetqval[2], mygetqval[6], mygetqval[7], mygetqval[8]);

	FreeIMU_AHRSupdate(mygetqval[3] * M_PI / 180, mygetqval[4] * M_PI / 180, mygetqval[5] * M_PI / 180,
			   mygetqval[0], mygetqval[1], mygetqval[2]);

	q[0] = qa0; //���ص�ǰֵ	FreeIMU_AHRSupdate �����������Ԫ�� ���õ�
	q[1] = qa1;
	q[2] = qa2;
	q[3] = qa3;
}

// a varient of asin() that checks the input ranges and ensures a
// valid angle as output. If nan is given as input then zero is
// returned.
float safe_asin(float v)
{
	if (isnan(v))
	{
		return 0.0f;
	}
	if (v >= 1.0f)
	{
		return M_PI / 2;
	}
	if (v <= -1.0f)
	{
		return -M_PI / 2;
	}
	return asin(v);
}


/**************************ʵ�ֺ���********************************************
*����ԭ��:	   void IMU_getYawPitchRoll(float * angles)
*��������:	 ������Ԫ�� ���ص�ǰ��������̬����
��������� ��Ҫ�����̬�ǵ������׵�ַ
���������û��
*******************************************************************************/
void IMU_getYawPitchRoll(float *angles)
{
	static float q[4]; //����Ԫ��
	IMU_getQ(q); //����ȫ����Ԫ��
	//	angles[2] = IMU_Roll = ACC_Roll;
	//	angles[1] = IMU_Pitch = ACC_Pitch;
	//  angles[0] = IMU_Yaw;
	IMU_Roll = angles[2] = (atan2(2.0f * (q[0] * q[1] + q[2] * q[3]),
				      1 - 2.0f * (q[1] * q[1] + q[2] * q[2]))) * 180 / M_PI;
	// we let safe_asin() handle the singularities near 90/-90 in pitch
	IMU_Pitch = angles[1] = -safe_asin(2.0f * (q[0] * q[2] - q[3] * q[1])) * 180 / M_PI;

	IMU_Yaw = angles[0] = -atan2(2 * q1 * q2 + 2 * q0 * q3, -2 * q2 * q2 - 2 * q3 * q3 + 1) * 180 / M_PI; // yaw

	//	if(IMU_Yaw <0)IMU_Yaw +=360.0f;  //�� -+180��  ת��0-360��
}
/**************************ʵ�ֺ���********************************************
*����ԭ��:	   void Initialize_Q(void)
*��������:  �ü��ٶȺ����̳�ʼ����Ԫ��
���������û��
���������û��
*******************************************************************************/
static float acc[9];
void Initialize_Q()
{
	int i;
	volatile float temp, roll, pitch, yaw, yh, xh;
	for(i = 0; i < DATA_SIZE; i++)
	{
		IMU_getValues(acc);
		acc_X += acc[0];
		acc_Y += acc[1];
		acc_Z += acc[2];
		acc_MX += acc[6];
		acc_MY += acc[7];
		acc_MZ += acc[8];
	}
	acc_X /= DATA_SIZE;
	acc_Y /= DATA_SIZE;
	acc_Z /= DATA_SIZE;
	acc_MX /= DATA_SIZE;
	acc_MY /= DATA_SIZE;
	acc_MZ /= DATA_SIZE;

	temp = acc_X * invSqrt((acc_Y * acc_Y + acc_Z * acc_Z));
	pitch = atan(temp) * 57.3;

	temp = acc_Y * invSqrt((acc_X * acc_X + acc_Z * acc_Z));
	roll = atan(temp) * 57.3;

	yh = acc_MY * cos(roll) + acc_MZ * sin(roll);
	xh = acc_MX * cos(pitch) + acc_MY * sin(roll) * sin(pitch) - acc_MZ * cos(roll) * sin(pitch);
	yaw = atan2(yh, xh);
	//��ʼ����Ԫ����ŷ����ת��Ԫ��
	q0 = cos(roll / 2) * cos(pitch / 2) * cos(yaw / 2) + sin(roll / 2) * sin(pitch / 2) * sin(yaw / 2);
	q1 = sin(roll / 2) * cos(pitch / 2) * cos(yaw / 2) - cos(roll / 2) * sin(pitch / 2) * sin(yaw / 2);
	q2 = cos(roll / 2) * sin(pitch / 2) * cos(yaw / 2) + sin(roll / 2) * cos(pitch / 2) * sin(yaw / 2);
	q3 = cos(roll / 2) * cos(pitch / 2) * sin(yaw / 2) - sin(roll / 2) * sin(pitch / 2) * cos(yaw / 2);
}

//------------------End of File----------------------------
