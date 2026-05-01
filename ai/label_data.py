"""
label_data.py — 标注传感器数据
用法: python label_data.py
输入: sensor_data.csv（从板子导出的原始数据）
输出: labeled_data.csv（带标签，0=正常，1=异常）
"""
import pandas as pd
import sys
import os

CSV_PATH = os.path.join(os.path.dirname(__file__), "sensor_data.csv")
OUT_PATH = os.path.join(os.path.dirname(__file__), "labeled_data.csv")

def main():
    if not os.path.exists(CSV_PATH):
        print(f"错误: 找不到 {CSV_PATH}")
        print("请先把板子上的数据导出放到 ai/ 目录下")
        sys.exit(1)

    df = pd.read_csv(CSV_PATH)
    print(f"总数据量: {len(df)} 条")
    print(f"列名: {list(df.columns)}")
    print(f"\n数据统计:")
    print(df.describe())

    df["label"] = 0

    # ========== 规则标注 ==========
    # 温度异常：高于 35°C
    df.loc[df["temp"] > 35, "label"] = 1

    # 振动异常：任一轴绝对值超过 5000（静止时一般在 ±200 以内，Z轴约 16000）
    df.loc[df["vib_x"].abs() > 5000, "label"] = 1
    df.loc[df["vib_y"].abs() > 5000, "label"] = 1
    # Z轴异常：偏离静止值 16000 超过 5000
    df.loc[(df["vib_z"] - 16000).abs() > 5000, "label"] = 1

    # 湿度突变：超过 80%（正常室内一般 40~70%）
    df.loc[df["humi"] > 80, "label"] = 1

    # ========== 手动标注（可选）==========
    # 如果你记录了制造异常的时间段，取消下面的注释并填入时间范围
    # df.loc[(df["timestamp"] >= 开始毫秒) & (df["timestamp"] <= 结束毫秒), "label"] = 1

    normal_count = (df["label"] == 0).sum()
    anomaly_count = (df["label"] == 1).sum()
    print(f"\n标注结果:")
    print(f"  正常: {normal_count} 条 ({normal_count/len(df)*100:.1f}%)")
    print(f"  异常: {anomaly_count} 条 ({anomaly_count/len(df)*100:.1f}%)")

    if anomaly_count < 50:
        print("\n⚠ 异常数据太少！建议:")
        print("  1. 重新运行程序，人为制造更多异常（捂热、摇晃、哈气）")
        print("  2. 或者调整上面的标注阈值（降低判定标准）")

    df.to_csv(OUT_PATH, index=False)
    print(f"\n已保存到 {OUT_PATH}")

if __name__ == "__main__":
    main()
